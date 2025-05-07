#pragma once

#include "udp_addr.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>
#include <utility>

namespace pulse::net {

class UDPSocket {
public:
    virtual ~UDPSocket() = default;

    // Send a packet to the given address
    virtual bool sendTo(const UDPAddr& addr, const uint8_t* data, size_t length) = 0;

    // Send a packet to the connected address
    virtual bool send(const uint8_t* data, size_t length) = 0;

    // Read a packet into the buffer and return the source address and number of bytes read
    // Returns false on non-blocking no-data or error
    virtual std::optional<std::tuple<const uint8_t*, size_t, UDPAddr>> recvFrom() = 0;

    // Returns underlying socket fd/handle if needed
    virtual int getHandle() const = 0;

    // Close the socket
    virtual void close() = 0;
};

// Factory
std::unique_ptr<UDPSocket> ListenUDP(const UDPAddr& bindAddr);
std::unique_ptr<UDPSocket> DialUDP(const UDPAddr& remoteAddr);

}