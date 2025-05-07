#include <pulseudp/udp.h>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <atomic>

int handleServer() {
    pulse::net::UDPAddr serverAddr("127.0.0.1", 9000);
    auto server = pulse::net::ListenUDP(serverAddr);

    std::unordered_map<std::string, int> clientDatagramCount;
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        try {
            if (auto maybe = server->recvFrom()) {
                auto [data, from] = *maybe;
                std::string clientKey = from.ip + ":" + std::to_string(from.port);
                clientDatagramCount[clientKey]++;

                // Echo the datagram back to the client
                if (!server->sendTo(from, data)) {
                    // This is a pretty fatal error since we're on loopback.
                    std::cerr << "Failed to send datagram back to client" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            // No need to handle this, just continue
        }
    }

    return 0;
}

void sendDatagrams(const std::string& serverIp, uint16_t serverPort, std::atomic<uint64_t>* datagramCount, std::atomic<uint64_t>* datagramTimeoutCount, std::atomic<bool>* stopFlag) {
    pulse::net::UDPAddr serverAddr(serverIp, serverPort);
    try {
        auto client = pulse::net::DialUDP(serverAddr);

        bool waitingForResponse = false;
        auto timeSinceLastSend = std::chrono::steady_clock::now();
        auto maxWaitTime = std::chrono::milliseconds(1000 / 48); // 24 Hz is the server tick rate

        std::vector<uint8_t> message = {'h', 'e', 'l', 'l', 'o'};
        while (!stopFlag->load()) {
            if (waitingForResponse && std::chrono::steady_clock::now() < timeSinceLastSend + maxWaitTime) {
                // Wait for a response from the server
                if (auto maybe = client->recvFrom()) {
                    auto [data, from] = *maybe;
                    waitingForResponse = false;
                    datagramCount->fetch_add(1); // Increment the datagram count only when a response is received
                }
            }
            else if (waitingForResponse) {
                // Timeout waiting for a response, send again
                waitingForResponse = false;
                datagramTimeoutCount->fetch_add(1);
            }
            else {
                // Sleep until the next tick
                std::this_thread::sleep_for(maxWaitTime - (std::chrono::steady_clock::now() - timeSinceLastSend));
                timeSinceLastSend = std::chrono::steady_clock::now();

                // Send a datagram to the server
                if (client->send(message)) {
                    waitingForResponse = true;
                } else {
                    std::cerr << "Failed to send datagram" << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int handleClient(int concurrentClients = 10) {
    const int numClients = concurrentClients;
    std::atomic<uint64_t> datagramsSent(0);
    std::atomic<uint64_t> datagramsTimedOut(0);
    std::atomic<bool> stopFlag(false);

    std::vector<std::thread> clientThreads;

    auto stopSignal = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::cout << "Sending datagrams for 10 seconds on " << numClients << " clients..." << std::endl;
    for (int i = 0; i < numClients; ++i) {
        clientThreads.emplace_back(sendDatagrams, "127.0.0.1", 9000, &datagramsSent, &datagramsTimedOut, &stopFlag);
    }

    // Wait until the stop signal is reached
    std::this_thread::sleep_until(stopSignal);
    stopFlag.store(true);

    std::cout << "Stopping clients..." << std::endl;
    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::cout << "Total datagrams sent: " << datagramsSent.load() << std::endl;
    std::cout << "Average datagrams per second: " << (datagramsSent.load() / 10) << std::endl;
    std::cout << "Average datagrams per client per second: " << ((datagramsSent.load() / numClients) / 10) << std::endl;
    std::cout << "Tick Rate: 24 Hz" << std::endl;

    std::cout << "Total datagrams timed out: " << datagramsTimedOut.load() << std::endl;
    std::cout << "Average datagrams timed out per second: " << (datagramsTimedOut.load() / 10) << std::endl;
    std::cout << "Average datagrams timed out per client: " << (datagramsTimedOut.load() / numClients) << std::endl;

    return 0;
}

int main(int argc, char* argv[]) {
    // If we passed a --server argument, run the server
    if (argc > 1 && std::string(argv[1]) == "--server") {
        return handleServer();
    }

    // If we passed a --client argument, run the client
    else if (argc > 1 && std::string(argv[1]) == "--client") {
        if (argc > 2) {
            // If we passed a number of clients, use that
            int numClients = std::stoi(argv[2]);
            return handleClient(numClients);
        } else {
            return handleClient();
        }
    }
    
    // Otherwise, send help.
    else {
        std::cout << "Usage: " << argv[0] << " [--server | --client [numClients]]" << std::endl;
        std::cout << "  --server: Run the server" << std::endl;
        std::cout << "  --client: Run the client (default: 10 concurrent clients)" << std::endl;
        return 1;
    }
}
