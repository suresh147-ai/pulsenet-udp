#include "pulseudp/udp.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <optional>
#include <condition_variable>

#pragma comment(lib, "ws2_32.lib")

namespace pulse::net {

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
    if (--wsaRefCount == 0) {
        WSACleanup();
    }
}

constexpr size_t RING_BUFFER_SIZE = 1024;
constexpr size_t PACKET_BUFFER_SIZE = 2048;

struct Packet {
    std::vector<uint8_t> data;
    UDPAddr addr;
};

class UDPSocketWindows : public UDPSocket {
public:
    UDPSocketWindows(SOCKET sock, bool useIOCP)
        : sock_(sock), iocpEnabled_(useIOCP), shutdown_(false), head_(0), tail_(0)
    {
        if (useIOCP) {
            iocp_ = CreateIoCompletionPort(reinterpret_cast<HANDLE>(sock_), NULL, 0, 1);
            if (!iocp_) {
                throw std::runtime_error("CreateIoCompletionPort failed");
            }

            ring_.resize(RING_BUFFER_SIZE);
            for (auto& p : ring_) {
                p.data.resize(PACKET_BUFFER_SIZE);
            }

            recvThread_ = std::thread([this]() { this->recvLoop(); });
        }
    }

    ~UDPSocketWindows() override {
        shutdown_ = true;
        if (iocpEnabled_) {
            PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
            if (recvThread_.joinable()) {
                recvThread_.join();
            }
            CloseHandle(iocp_);
        }
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
        if (!iocpEnabled_) {
            // Basic recvfrom for DialUDP clients
            static thread_local char buf[PACKET_BUFFER_SIZE];
            sockaddr_storage src{};
            int srclen = sizeof(src);
    
            int received = ::recvfrom(sock_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&src), &srclen);
            if (received == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) return std::nullopt;
                throw std::runtime_error("recvfrom failed: " + std::to_string(err));
            }
    
            UDPAddr addr = decodeAddr(reinterpret_cast<sockaddr*>(&src));
            return std::make_tuple(reinterpret_cast<const uint8_t*>(buf), received, addr);
        }
    
        std::scoped_lock lock(mutex_);
        if (head_ == tail_) return std::nullopt;
    
        auto& pkt = ring_[tail_ % RING_BUFFER_SIZE];
        const uint8_t* ptr = pkt.data.data();
        size_t len = pkt.data.size();
        UDPAddr addr = pkt.addr;
    
        tail_++;
        return std::make_tuple(ptr, len, addr);
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
    HANDLE iocp_ = nullptr;
    bool iocpEnabled_;
    std::atomic_bool shutdown_;
    std::thread recvThread_;

    struct OverlappedEntry {
        WSAOVERLAPPED overlapped;
        WSABUF buffer;
        sockaddr_storage remoteAddr;
        int remoteLen;
        DWORD flags;
        size_t index;
    };

    std::vector<Packet> ring_;
    std::vector<OverlappedEntry> entries_;
    std::mutex mutex_;
    size_t head_;
    size_t tail_;

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

    void postReceive(size_t idx) {
        auto& entry = entries_[idx];
        memset(&entry.overlapped, 0, sizeof(entry.overlapped));
        entry.remoteLen = sizeof(entry.remoteAddr);
        entry.flags = 0;

        entry.buffer.buf = reinterpret_cast<char*>(ring_[idx].data.data());
        entry.buffer.len = static_cast<ULONG>(ring_[idx].data.size());

        DWORD bytesReceived = 0;
        int r = WSARecvFrom(
            sock_,
            &entry.buffer,
            1,
            &bytesReceived,
            &entry.flags,
            reinterpret_cast<sockaddr*>(&entry.remoteAddr),
            &entry.remoteLen,
            &entry.overlapped,
            nullptr
        );

        if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            throw std::runtime_error("WSARecvFrom failed: " + std::to_string(WSAGetLastError()));
        }
    }

    void recvLoop() {
        entries_.resize(RING_BUFFER_SIZE);
        for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
            entries_[i].index = i;
            postReceive(i);
        }

        while (!shutdown_) {
            DWORD bytes;
            ULONG_PTR key;
            LPOVERLAPPED overlapped;

            BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &overlapped, INFINITE);
            if (!ok || shutdown_ || !overlapped) continue;

            auto* entry = reinterpret_cast<OverlappedEntry*>(overlapped);
            auto& pkt = ring_[entry->index];

            pkt.data.resize(bytes);
            pkt.addr = decodeAddr(reinterpret_cast<sockaddr*>(&entry->remoteAddr));

            {
                std::scoped_lock lock(mutex_);
                ring_[(head_++) % RING_BUFFER_SIZE] = std::move(pkt);
            }

            postReceive(entry->index);
        }
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

    if (bind(sock, reinterpret_cast<const sockaddr*>(addrPtr), (family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) < 0) {
        closesocket(sock);
        throw std::runtime_error("bind() failed");
    }

    return std::make_unique<UDPSocketWindows>(sock, true);
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

    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        closesocket(sock);
        throw std::runtime_error("ioctlsocket() failed: " + std::to_string(WSAGetLastError()));
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&remoteSock), remoteLen) == SOCKET_ERROR) {
        closesocket(sock);
        throw std::runtime_error("connect() failed: " + std::to_string(WSAGetLastError()));
    }

    return std::make_unique<UDPSocketWindows>(sock, false);
}

} // namespace pulse::net
