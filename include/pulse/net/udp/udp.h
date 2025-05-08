#pragma once

#include "udp_addr.h"
#include "error_code.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>
#include <utility>

#if __cpp_lib_expected >= 202211L
    #include <expected>
#else
    #error "std::expected is not available. Upgrade your compiler."
#endif

namespace pulse::net::udp {

    struct ReceivedPacket {
        const uint8_t* data;
        size_t length;
        Addr addr;
    };

class Socket {
public:
    virtual ~Socket() = default;

    // Send a packet to the given address
    virtual std::expected<void, ErrorCode> sendTo(const Addr& addr, const uint8_t* data, size_t length) = 0;

    // Send a packet to the connected address
    virtual std::expected<void, ErrorCode> send(const uint8_t* data, size_t length) = 0;

    /// Receives a packet. The returned `data` pointer is valid only until the next recvFrom() call on the same thread.
    virtual std::expected<ReceivedPacket, ErrorCode> recvFrom() = 0;

    // Returns underlying socket fd/handle if needed
    virtual std::expected<int, ErrorCode> getHandle() const = 0;

    // Close the socket
    virtual void close() = 0;
};

// Factory
std::expected<std::unique_ptr<Socket>, ErrorCode> Listen(const Addr& bindAddr);
std::expected<std::unique_ptr<Socket>, ErrorCode> Dial(const Addr& remoteAddr);

}