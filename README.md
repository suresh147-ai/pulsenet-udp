<div align="center">
  <img src="https://pulsenet.dev/images/pulse-networking-social.png" alt="Pulse Networking" width="1200">
  <h1>pulseudp</h1>
  <p><strong>Raw non-blocking UDP sockets with Go-style ergonomics.</strong></p>
  <p>
    <a href="#features">Features</a> •
    <a href="#why">Why?</a> •
    <a href="#usage">Usage</a> •
    <a href="#platform-support">Platform Support</a> •
    <a href="#license">License</a>
  </p>
</div>

---

pulseudp is a cross-platform UDP socket wrapper for C++ that gives you a clean, Go-style interface — no blocking nonsense, no bloated event loops, and no libuv circus.

Use it as the foundation for anything low-latency and real-time: games, protocols, transport layers, etc.

It’s designed to get out of your way and let the OS do its job.

## Features
 - ✅ Non-blocking UDP I/O (just like Go)
 - ✅ Listen() for servers
 - ✅ Dial() for clients
 - ✅ sendTo() and recvFrom() like you’d expect
 - ✅ Optional send() and recv() with connected semantics
 - ✅ IPv4 + IPv6 support
 - ✅ No third-party dependencies
 - ✅ Portable between Unix and Windows
 - ✅ Easy to test, drop-in for higher-level protocols

## Why?

Because C++ UDP sockets are a mess:
 - On Unix, you’re writing raw socket()/bind()/recvfrom() boilerplate.
 - On Windows, you’re dancing with Winsock and WSAStartup() just to send a packet.
 - Nobody agrees on non-blocking behavior or even a clean API.

Go got it right. So we copied it.

This isn’t an abstraction layer built on top of a ten-ton framework. It’s just clean, minimal UDP done right.

## Usage

```cpp
#include <pulse/net/udp/udp.h>
#include <iostream>

int main() {
    pulse::net::udp::Addr serverAddr("127.0.0.1", 9000);
    auto server = pulse::net::udp::Listen(serverAddr);
    auto client = pulse::net::udp::Dial(serverAddr);

    std::vector<uint8_t> msg = {'h', 'e', 'l', 'l', 'o'};
    client->send(msg);

    std::optional<pulse::net::udp::Addr> from;
    std::vector<uint8_t> data;

    if (auto maybe = server->recvFrom()) {
        std::tie(data, from) = *maybe;
        std::cout << "Got packet from " << from->ip << ":" << from->port << std::endl;
    }

    return 0;
}
```

## Platform Support

 | Platform | Supported? | Impl Notes | 
 | ---      | ---        | ---        |
 | macOS    | ✅         | fcntl() non-blocking sockets |
 | Linux    | ✅         | Same as above |
 | Windows  | ✅         | Raw Winsock2 w/ WSAStartup() |

You don’t need libuv, boost, or some “cross-platform networking abstraction” from hell. It’s all native. It’s all simple.

## License

AGPLv3. If you’re using this in a closed-source project, you need to license your project AGPLv3 as well or purchase a commercial license.

## Final Word

pulseudp is not a toy. It’s not trying to hold your hand. It gives you raw UDP, cleanly wrapped, so you can go build real-time systems without dragging 200 build dependencies behind you.

If you want a networking library with 300 contributors and an official Discord server, you’re in the wrong place.

If you want simple, fast, understandable UDP for serious use — welcome.