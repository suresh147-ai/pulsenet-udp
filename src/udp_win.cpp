#include "pulseudp/udp.h"
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

namespace pulse::net {

    constexpr size_t PACKET_BUFFER_SIZE = 2048;

    static std::atomic<int> wsaRefCount{0};
    static std::mutex wsaMutex;

    static void initWSA() {
        std::lock_guard<std::mutex> lock(wsaMutex);
        if (wsaRefCount == 0) {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw std::runtime_error("WSAStartup failed");
            }
        }
        wsaRefCount++;
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

class UDPSocketWindows : public UDPSocket {
public:
    UDPSocketWindows(SOCKET sock) : sock_(sock) {
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

    ~UDPSocketWindows() override {
        close();
        cleanupWSA();
    }

    bool sendTo(const UDPAddr& addr, const uint8_t* data, size_t length) override {
        int sent = sendto(
            sock_,
            reinterpret_cast<const char*>(data),
            static_cast<int>(length),
            0,
            reinterpret_cast<const sockaddr*>(addr.sockaddrData()),
            static_cast<int>(addr.sockaddrLen())
        );
        return sent == static_cast<int>(length);
    }

    bool send(const uint8_t* data, size_t length) override {
        int sent = ::send(sock_, reinterpret_cast<const char*>(data), static_cast<int>(length), 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                return false;
            }
            throw std::runtime_error("send failed: " + std::to_string(err));
        }
        return sent == static_cast<int>(length);
    }

    std::optional<std::tuple<const uint8_t*, size_t, UDPAddr>> recvFrom() override {
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
            if (err == WSAEWOULDBLOCK) {
                return std::nullopt;
            }
            throw std::runtime_error("recvfrom failed: " + std::to_string(err));
        }
    
        UDPAddr addr = decodeAddr(reinterpret_cast<sockaddr*>(&src));
        return std::make_tuple(static_cast<uint8_t*>(buf), static_cast<size_t>(received), addr);
    }

    int getHandle() const override {
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

    static UDPAddr decodeAddr(const sockaddr* addr) {
        char ip[INET6_ADDRSTRLEN];
        uint16_t port = 0;

        if (addr->sa_family == AF_INET) {
            auto* a = reinterpret_cast<const sockaddr_in*>(addr);
            inet_ntop(AF_INET, &a->sin_addr, ip, sizeof(ip));
            port = ntohs(a->sin_port);
        } else if (addr->sa_family == AF_INET6) {
            auto* a = reinterpret_cast<const sockaddr_in6*>(addr);
            inet_ntop(AF_INET6, &a->sin6_addr, ip, sizeof(ip));
            port = ntohs(a->sin6_port);
        } else {
            throw std::runtime_error("Unsupported address family");
        }

        return UDPAddr(ip, port);
    }
};

std::unique_ptr<UDPSocket> ListenUDP(const UDPAddr& bindAddr) {
    initWSA();

    int family = AF_INET;
    const void* addrPtr = nullptr;

    sockaddr_in addr4{};
    if (inet_pton(AF_INET, bindAddr.ip.c_str(), &addr4.sin_addr) == 1) {
        family = AF_INET;
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(bindAddr.port);
        addrPtr = &addr4;
    } else {
        sockaddr_in6 addr6{};
        if (inet_pton(AF_INET6, bindAddr.ip.c_str(), &addr6.sin6_addr) != 1) {
            throw std::runtime_error("Invalid IP address: " + bindAddr.ip);
        }
        family = AF_INET6;
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(bindAddr.port);
        addrPtr = &addr6;
    }

    SOCKET sock = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
    }

    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        throw std::runtime_error("ioctlsocket() failed");
    }

    if (bind(sock, reinterpret_cast<const sockaddr*>(addrPtr), (family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) < 0) {
        closesocket(sock);
        throw std::runtime_error("bind() failed");
    }

    return std::make_unique<UDPSocketWindows>(sock);
}

std::unique_ptr<UDPSocket> DialUDP(const UDPAddr& remoteAddr) {
    initWSA();

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
        throw std::runtime_error("Invalid IP address: " + remoteAddr.ip);
    }

    SOCKET sock = socket(family, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("socket() failed: " + std::to_string(WSAGetLastError()));
    }

    // Non-blocking
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        throw std::runtime_error("ioctlsocket() failed: " + std::to_string(WSAGetLastError()));
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&remoteSock), remoteLen) == SOCKET_ERROR) {
        closesocket(sock);
        throw std::runtime_error("connect() failed: " + std::to_string(WSAGetLastError()));
    }

    return std::make_unique<UDPSocketWindows>(sock);
}

} // namespace pulse::net