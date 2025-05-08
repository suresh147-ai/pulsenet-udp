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

private:
    alignas(16) char storage_[128]{}; // big enough for sockaddr_in6
};

} // namespace pulse::net::udp