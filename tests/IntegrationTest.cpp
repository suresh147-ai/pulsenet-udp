#include <iostream>
#include <pulseudp/udp.h>

int main () {
    std::cout << "Creating a server to receive packets..." << std::endl;
    pulse::net::UDPAddr serverAddr("127.0.0.1", 12345);
    auto serverSocket = pulse::net::ListenUDP(serverAddr);
    if (!serverSocket) {
        std::cerr << "Failed to create server socket." << std::endl;
        return 1;
    }
    std::cout << "Server socket created successfully." << std::endl;

    std::cout << "Creating a client to send packets..." << std::endl;
    auto clientSocket = pulse::net::DialUDP(serverAddr);
    if (!clientSocket) {
        std::cerr << "Failed to create client socket." << std::endl;
        return 1;
    }
    std::cout << "Client socket created successfully." << std::endl;

    std::string message = "Hello, UDP!";
    std::vector<uint8_t> data(message.begin(), message.end());
    if (!clientSocket->send(data.data(), data.size())) {
        std::cerr << "Failed to send data." << std::endl;
        return 1;
    }
    std::cout << "Data sent successfully." << std::endl;

    if (auto maybe = serverSocket->recvFrom()) {
        auto [data, length, addr] = *maybe;
        std::string receivedMessage(reinterpret_cast<const char*>(data), length);
        std::cout << "Received message: " << receivedMessage << " from " << addr.ip << ":" << addr.port << std::endl;
        if (receivedMessage != message) {
            std::cerr << "Received message does not match sent message." << std::endl;
            return 1;
        }
        std::cout << "Received message matches sent message." << std::endl;
    }
    else {
        std::cerr << "Failed to receive data." << std::endl;
        return 1;
    }
    std::cout << "Test completed successfully." << std::endl;
    return 0;
}