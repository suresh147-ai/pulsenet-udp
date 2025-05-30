#include <iostream>
#include <pulse/net/udp/udp.h>

int main () {
    using namespace pulse::net::udp;

    std::cout << "Creating a server to receive packets..." << std::endl;
    Addr serverAddr("127.0.0.1", 12345);
    auto serverSocketResult = Listen(serverAddr);
    if (!serverSocketResult) {
        std::cerr << "Failed to create server socket: " << static_cast<int>(serverSocketResult.error()) << std::endl;
        return 1;
    }
    auto& serverSocket = *serverSocketResult;
    std::cout << "Server socket created successfully." << std::endl;

    std::cout << "Creating a client to send packets..." << std::endl;
    auto clientSocketResult = Dial(serverAddr);
    if (!clientSocketResult) {
        std::cerr << "Failed to create client socket: " << static_cast<int>(clientSocketResult.error()) << std::endl;
        return 1;
    }
    auto& clientSocket = *clientSocketResult;
    std::cout << "Client socket created successfully." << std::endl;

    std::string message = "Hello, UDP!";
    std::vector<uint8_t> data(message.begin(), message.end());

    auto sendResult = clientSocket->send(data.data(), data.size());
    if (!sendResult) {
        std::cerr << "Failed to send data: " << static_cast<int>(sendResult.error()) << std::endl;
        return 1;
    }
    std::cout << "Data sent successfully." << std::endl;

    auto recvResult = serverSocket->recvFrom();
    if (!recvResult) {
        std::cerr << "Failed to receive data: " << static_cast<int>(recvResult.error()) << std::endl;
        return 1;
    }

    const auto& [recvData, length, addr] = *recvResult;
    std::string receivedMessage(reinterpret_cast<const char*>(recvData), length);
    std::cout << "Received message: " << receivedMessage << " from " << addr.ip << ":" << addr.port << std::endl;

    if (receivedMessage != message) {
        std::cerr << "Received message does not match sent message." << std::endl;
        return 1;
    }

    std::cout << "Received message matches sent message." << std::endl;
    std::cout << "Test completed successfully." << std::endl;
    return 0;
}
