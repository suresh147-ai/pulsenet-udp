#include "pulse/net/udp/udp.h"
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

namespace pulse::net::udp {

    constexpr size_t PACKET_BUFFER_SIZE = 2048;

    static std::atomic<int> wsaRefCount{0};
    static std::mutex wsaMutex;

    static std::expected<void, ErrorCode> initWSA() {
        std::lock_guard<std::mutex> lock(wsaMutex);
        if (wsaRefCount == 0) {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                return std::unexpected(ErrorCode::WSAStartupFailed);
            }
        }
        ++wsaRefCount;
        return {};
    }


    static void cleanupWSA() {
        std::lock_guard<std::mutex> lock(wsaMutex);
        if (wsaRefCount > 0) {
            wsaRefCount--;
            if (wsaRefCount == 0) {
                WSACleanup();
            }
        }
    }

class SocketWindows : public Socket {
public:
    SocketWindows(SOCKET sock) : sock_(sock) {
        // Increase socket buffer sizes but don't start receiving thread
        int sendBufSize = 4 * 1024 * 1024; // 4MB
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
        
        // We still need a reasonable receive buffer for any responses
        int recvBufSize = 1 * 1024 * 1024; // 1MB is enough for client
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize));
        
        // Disable connection reset behavior
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        WSAIoctl(sock, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), 
                NULL, 0, &dwBytesReturned, NULL, NULL);
    }

    ~SocketWindows() override {
        close();
        cleanupWSA();
    }

    inline std::unexpected<ErrorCode> mapWSASendError(int err) {
        switch (err) {
            case WSAEWOULDBLOCK: return std::unexpected(ErrorCode::WouldBlock);
            case WSAENOTSOCK:
            case WSAEBADF:       return std::unexpected(ErrorCode::InvalidSocket);
            case WSAECONNRESET:  return std::unexpected(ErrorCode::ConnectionReset);
            default:             return std::unexpected(ErrorCode::SendFailed);
        }
    }
    
    inline std::unexpected<ErrorCode> mapWSARecvError(int err) {
        switch (err) {
            case WSAEWOULDBLOCK: return std::unexpected(ErrorCode::WouldBlock);
            case WSAENOTSOCK:
            case WSAEBADF:       return std::unexpected(ErrorCode::InvalidSocket);
            default:             return std::unexpected(ErrorCode::RecvFailed);
        }
    }

    std::expected<void, ErrorCode> sendTo(const Addr& addr, const uint8_t* data, size_t length) override {
        int sent = ::sendto(
            sock_,
            reinterpret_cast<const char*>(data),
            static_cast<int>(length),
            0,
            reinterpret_cast<const sockaddr*>(addr.sockaddrData()),
            static_cast<int>(addr.sockaddrLen())
        );
    
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            return mapWSASendError(err);
        }
    
        if (sent != static_cast<int>(length)) {
            return std::unexpected(ErrorCode::PartialSend);
        }
    
        return {};
    }

    std::expected<void, ErrorCode> send(const uint8_t* data, size_t length) override {
        int sent = ::send(sock_, reinterpret_cast<const char*>(data), static_cast<int>(length), 0);
    
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            return mapWSASendError(err);
        }
    
        if (sent != static_cast<int>(length)) {
            return std::unexpected(ErrorCode::PartialSend);
        }
    
        return {};
    }

    std::expected<ReceivedPacket, ErrorCode> recvFrom() override {
        static thread_local uint8_t buf[PACKET_BUFFER_SIZE];
        sockaddr_storage src{};
        int srclen = sizeof(src);
    
        int received = ::recvfrom(
            sock_,
            reinterpret_cast<char*>(buf),
            sizeof(buf),
            0,
            reinterpret_cast<sockaddr*>(&src),
            &srclen
        );
    
        if (received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            return mapWSARecvError(err);
        }
    
        auto addr = decodeAddr(reinterpret_cast<sockaddr*>(&src));
        if (!addr) {
            return std::unexpected(addr.error());
        }
    
        if (addr->port == 0) {
            return std::unexpected(ErrorCode::InvalidAddress);
        }
    
        return ReceivedPacket{
            .data = reinterpret_cast<const uint8_t*>(buf),
            .length = static_cast<size_t>(received),
            .addr = std::move(*addr)
        };
    }
    
    std::expected<int, ErrorCode> getHandle() const override {
        if (sock_ == INVALID_SOCKET) {
            return std::unexpected(ErrorCode::InvalidSocket);
        }
        return static_cast<int>(sock_);
    }
    

    void close() override {
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }
    }

private:
    SOCKET sock_;

    static std::expected<Addr, ErrorCode> decodeAddr(const sockaddr* addr) {
        char ip[INET6_ADDRSTRLEN];
        uint16_t port = 0;
    
        if (addr->sa_family == AF_INET) {
            auto* a = reinterpret_cast<const sockaddr_in*>(addr);
            if (!inet_ntop(AF_INET, &a->sin_addr, ip, sizeof(ip))) {
                return std::unexpected(ErrorCode::InvalidAddress);
            }
            port = ntohs(a->sin_port);
        } else if (addr->sa_family == AF_INET6) {
            auto* a = reinterpret_cast<const sockaddr_in6*>(addr);
            if (!inet_ntop(AF_INET6, &a->sin6_addr, ip, sizeof(ip))) {
                return std::unexpected(ErrorCode::InvalidAddress);
            }
            port = ntohs(a->sin6_port);
        } else {
            return std::unexpected(ErrorCode::UnsupportedAddressFamily);
        }
    
        return Addr{ip, port};
    }
    
};

std::expected<std::unique_ptr<Socket>, ErrorCode> Listen(const Addr& bindAddr) {
    if (auto err = initWSA(); !err) {
        return std::unexpected(err.error());
    }

    int family = AF_INET;
    const void* addrPtr = nullptr;

    sockaddr_in addr4{};
    sockaddr_in6 addr6{};

    if (inet_pton(AF_INET, bindAddr.ip.c_str(), &addr4.sin_addr) == 1) {
        family = AF_INET;
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(bindAddr.port);
        addrPtr = &addr4;
    } else if (inet_pton(AF_INET6, bindAddr.ip.c_str(), &addr6.sin6_addr) == 1) {
        family = AF_INET6;
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(bindAddr.port);
        addrPtr = &addr6;
    } else {
        return std::unexpected(ErrorCode::InvalidAddress);
    }

    SOCKET sock = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return std::unexpected(ErrorCode::SocketCreateFailed);
    }

    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        return std::unexpected(ErrorCode::SocketConfigFailed);
    }

    int result = bind(
        sock,
        reinterpret_cast<const sockaddr*>(addrPtr),
        (family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)
    );

    if (result < 0) {
        closesocket(sock);
        return std::unexpected(ErrorCode::BindFailed);
    }

    return std::make_unique<SocketWindows>(sock);
}

std::expected<std::unique_ptr<Socket>, ErrorCode> Dial(const Addr& remoteAddr) {
    if (auto err = initWSA(); !err) {
        return std::unexpected(err.error());
    }

    int family = AF_INET;
    sockaddr_storage remoteSock{};
    int remoteLen = 0;

    if (InetPtonA(AF_INET, remoteAddr.ip.c_str(), &reinterpret_cast<sockaddr_in*>(&remoteSock)->sin_addr) == 1) {
        auto* addr4 = reinterpret_cast<sockaddr_in*>(&remoteSock);
        family = AF_INET;
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(remoteAddr.port);
        remoteLen = sizeof(sockaddr_in);
    } else if (InetPtonA(AF_INET6, remoteAddr.ip.c_str(), &reinterpret_cast<sockaddr_in6*>(&remoteSock)->sin6_addr) == 1) {
        auto* addr6 = reinterpret_cast<sockaddr_in6*>(&remoteSock);
        family = AF_INET6;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(remoteAddr.port);
        remoteLen = sizeof(sockaddr_in6);
    } else {
        return std::unexpected(ErrorCode::InvalidAddress);
    }

    SOCKET sock = socket(family, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        return std::unexpected(ErrorCode::SocketCreateFailed);
    }

    // Non-blocking
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        return std::unexpected(ErrorCode::SocketConfigFailed);
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&remoteSock), remoteLen) == SOCKET_ERROR) {
        closesocket(sock);
        return std::unexpected(ErrorCode::ConnectFailed);
    }

    return std::make_unique<SocketWindows>(sock);
}


} // namespace pulse::net::udp