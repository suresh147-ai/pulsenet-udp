#pragma once

namespace pulse::net::udp {

    enum class ErrorCode {
        None = 0,
        InvalidAddress,
        SocketCreationFailed,
        BindFailed,
        ConnectFailed,
        RecvFailed,
        SendFailed,
        Timeout,
        Closed,
        WouldBlock,
        InvalidSocket,
        ConnectionReset,
        PartialSend,
        UnsupportedAddressFamily,
        SocketCreateFailed,
        SocketConfigFailed,
        WSAStartupFailed,
        Unknown = 9999
    };

    constexpr const char* ErrorToString(ErrorCode code) {
        switch (code) {
            case ErrorCode::None: return "No error";
            case ErrorCode::InvalidAddress: return "Invalid address";
            case ErrorCode::SocketCreationFailed: return "Socket creation failed";
            case ErrorCode::BindFailed: return "Bind failed";
            case ErrorCode::ConnectFailed: return "Connect failed";
            case ErrorCode::RecvFailed: return "Receive failed";
            case ErrorCode::SendFailed: return "Send failed";
            case ErrorCode::Timeout: return "Timeout";
            case ErrorCode::Closed: return "Socket closed";
            case ErrorCode::WouldBlock: return "Operation would block";
            case ErrorCode::InvalidSocket: return "Invalid socket";
            case ErrorCode::ConnectionReset: return "Connection reset";
            case ErrorCode::PartialSend: return "Partial send";
            case ErrorCode::UnsupportedAddressFamily: return "Unsupported address family";
            case ErrorCode::SocketCreateFailed: return "Socket creation failed";
            case ErrorCode::SocketConfigFailed: return "Socket configuration failed";
            case ErrorCode::WSAStartupFailed: return "WSAStartup failed";
            default: return "Unknown error";
        }
    }

} // namespace pulse::net::udp