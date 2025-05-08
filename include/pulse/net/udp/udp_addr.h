#pragma once

#include <string>
#include <cstdint>

namespace pulse::net::udp {

class Addr {
public:
    Addr() = default;
    Addr(const std::string& ip, uint16_t port);

    std::string ip;
    uint16_t port;

    // Internal socket address access
    const void* sockaddrData() const;
    size_t sockaddrLen() const;

    // Let's create a constant AnyIPv4 and AnyIPv6 address const char * for convenience
    static const char * AnyIPv4;
    static const char * AnyIPv6;

private:
    alignas(16) char storage_[128]{}; // big enough for sockaddr_in6
};

inline bool operator==(const Addr& lhs, const Addr& rhs) {
    return lhs.ip == rhs.ip && lhs.port == rhs.port;
}

} // namespace pulse::net::udp

namespace std {
    template <>
    struct hash<pulse::net::udp::Addr> {
        size_t operator()(const pulse::net::udp::Addr& a) const noexcept {
            size_t h1 = std::hash<std::string>{}(a.ip);
            size_t h2 = std::hash<uint16_t>{}(a.port);
            return h1 ^ (h2 << 1);
        }
    };
}
