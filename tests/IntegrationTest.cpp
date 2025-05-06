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
    if (!clientSocket->send(data)) {
        std::cerr << "Failed to send data." << std::endl;
        return 1;
    }
    std::cout << "Data sent successfully." << std::endl;

    auto received = serverSocket->recvFrom();
    if (!received) {
        std::cerr << "Failed to receive data." << std::endl;
        return 1;
    }
    auto [recvData, addr] = *received;
    std::string recvMessage(recvData.begin(), recvData.end());
    std::cout << "Received message: " << recvMessage << " from " << addr.ip << ":" << addr.port << std::endl;
    if (recvMessage != message) {
        std::cerr << "Received message does not match sent message." << std::endl;
        return 1;
    }
    std::cout << "Received message matches sent message." << std::endl;
    std::cout << "Test completed successfully." << std::endl;
    return 0;
}