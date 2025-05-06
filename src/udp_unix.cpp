#include "pulseudp/udp.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

namespace pulse::net {

class UDPSocketUnix : public UDPSocket {
public:
    UDPSocketUnix(int sockfd) : sockfd_(sockfd) {}
    ~UDPSocketUnix() override {
        close();
    }

    bool sendTo(const UDPAddr& addr, const std::vector<uint8_t>& data) override {
        ssize_t sent = sendto(
            sockfd_,
            data.data(),
            data.size(),
            0,
            reinterpret_cast<const sockaddr*>(addr.sockaddrData()),
            static_cast<socklen_t>(addr.sockaddrLen())
        );
        return sent == static_cast<ssize_t>(data.size());
    }

    bool send(const std::vector<uint8_t>& data) override {
        ssize_t sent = ::send(sockfd_, data.data(), data.size(), 0);
        if (sent < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return false;
            }
            throw std::runtime_error("send failed: " + std::string(strerror(errno)));
        }
        return sent == static_cast<ssize_t>(data.size());
    }

    std::optional<std::pair<std::vector<uint8_t>, UDPAddr>> recvFrom() override {
        char buf[2048];
        sockaddr_storage src{};
        socklen_t srclen = sizeof(src);
    
        ssize_t received = ::recvfrom(
            sockfd_,
            buf,
            sizeof(buf),
            0,
            reinterpret_cast<sockaddr*>(&src),
            &srclen
        );
    
        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return std::nullopt;
            }
            throw std::runtime_error("recvfrom failed: " + std::string(strerror(errno)));
        }
    
        std::vector<uint8_t> data(buf, buf + received);
        UDPAddr addr = decodeAddr(reinterpret_cast<sockaddr*>(&src));
        return std::make_pair(std::move(data), std::move(addr));
    }

    int getHandle() const override {
        return sockfd_;
    }

    void close() override {
        if (sockfd_ != -1) {
            ::close(sockfd_);
            sockfd_ = -1;
        }
    }

private:
    int sockfd_;

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

    int sockfd = socket(family, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));
    }

    // Make socket non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Bind
    if (bind(sockfd, reinterpret_cast<const sockaddr*>(addrPtr), (family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) < 0) {
        close(sockfd);
        throw std::runtime_error("bind() failed: " + std::string(strerror(errno)));
    }

    return std::make_unique<UDPSocketUnix>(sockfd);
}

std::unique_ptr<UDPSocket> DialUDP(const UDPAddr& remoteAddr) {
    int family = AF_INET;
    sockaddr_storage remoteSock{};
    socklen_t remoteLen = 0;

    if (inet_pton(AF_INET, remoteAddr.ip.c_str(), &reinterpret_cast<sockaddr_in*>(&remoteSock)->sin_addr) == 1) {
        auto* addr4 = reinterpret_cast<sockaddr_in*>(&remoteSock);
        family = AF_INET;
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(remoteAddr.port);
        remoteLen = sizeof(sockaddr_in);
    } else if (inet_pton(AF_INET6, remoteAddr.ip.c_str(), &reinterpret_cast<sockaddr_in6*>(&remoteSock)->sin6_addr) == 1) {
        auto* addr6 = reinterpret_cast<sockaddr_in6*>(&remoteSock);
        family = AF_INET6;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(remoteAddr.port);
        remoteLen = sizeof(sockaddr_in6);
    } else {
        throw std::runtime_error("Invalid IP address: " + remoteAddr.ip);
    }

    int sockfd = socket(family, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));
    }

    // Non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&remoteSock), remoteLen) < 0) {
        ::close(sockfd);
        throw std::runtime_error("connect() failed: " + std::string(strerror(errno)));
    }

    return std::make_unique<UDPSocketUnix>(sockfd);
}

} // namespace pulse::net