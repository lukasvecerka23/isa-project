// tftp_server.cpp
#include "server/tftp_server.hpp"
#include "common/packets.hpp"
#include "common/session.hpp"
#include "common/exceptions.hpp"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <future>

int bind_new_socket(){
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Failed to open socket");
    }
    // Initialize server address structure
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
    server_addr.sin_port = htons(0);

    // Bind socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to bind socket to port");
    }

    return sockfd;
}

void TFTPServer::shutDown() {
    // Wait for all client threads to finish
    for (auto& future : clientFutures) {
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::timeout) {
            future.get(); // This will block until the future is ready
            std::cout << "Client session terminated from SIGINT" << std::endl;
        }
    }

    // Clear the vector of futures
    clientFutures.clear();

    close(sockfd);

    exit(0);
}

void TFTPServer::start() {
    std::cout << "Server listening on port " << port << std::endl;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    // Enter a loop to receive data
    while (true) {
        // Receive initial request from a clients
        ssize_t received_bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                          (struct sockaddr *)&client_addr, &client_len);
        if (received_bytes < 0) {
            if (stopFlag->load()){
                std::cout << "Stopping server..." << std::endl;
                shutDown();
               return;
            }
            continue;
        }

        // Run new thread for each client
        auto future = std::async(std::launch::async, &TFTPServer::handleClientRequest, this, client_addr, buffer, received_bytes);
        clientFutures.push_back(std::move(future));

        // Remove finished futures
        clientFutures.erase(std::remove_if(clientFutures.begin(), clientFutures.end(), [](const std::future<void>& f) {
            return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }), clientFutures.end());
    }
}

void TFTPServer::handleClientRequest(const sockaddr_in& clientAddr, const char* buffer, ssize_t bufferSize) {
    // Parse the packet
    std::unique_ptr<Packet> packet;
    try {
        packet = Packet::parse(clientAddr, buffer, bufferSize);
    }
    catch (const ParsingError& e) {
        ErrorPacket errorPacket(ParsingError::errorCode, e.what());
        errorPacket.send(sockfd, clientAddr);
        return;
    }
    catch (const OptionError& e) {
        ErrorPacket errorPacket(OptionError::errorCode, e.what());
        errorPacket.send(sockfd, clientAddr);
        return;
    }
    catch (const std::exception& e) {
        ErrorPacket errorPacket(0, e.what());
        errorPacket.send(sockfd, clientAddr);
        return;
    }

    // Handle the packet
    switch (packet->getOpcode()){
        case 1: // RRQ
        {
            int sockfd = bind_new_socket();
            ReadRequestPacket* readPacket = dynamic_cast<ReadRequestPacket*>(packet.get());
            readPacket->filename = rootDirPath + "/" + readPacket->filename;
            if (!std::filesystem::exists(readPacket->filename)){
                ErrorPacket errorPacket(1, "File not found");
                errorPacket.send(sockfd, clientAddr);
                close(sockfd);
                break;
            }
            ServerSession readSession(sockfd, clientAddr, readPacket->filename, "", readPacket->mode, SessionType::READ, readPacket->options);
            readSession.handleSession();
            break;
        }
        case 2: // WRQ
        {
            int sockfd = bind_new_socket();
            WriteRequestPacket* writePacket = dynamic_cast<WriteRequestPacket*>(packet.get());
            writePacket->filename = rootDirPath + "/" + writePacket->filename;
            if (std::filesystem::exists(writePacket->filename)){
                ErrorPacket errorPacket(6, "File already exists");
                errorPacket.send(sockfd, clientAddr);
                close(sockfd);
                break;
            }
            ServerSession writeSession(sockfd, clientAddr, "", writePacket->filename, writePacket->mode, SessionType::WRITE, writePacket->options);
            writeSession.handleSession();
            break;
        }
        default:
            ErrorPacket errorPacket(4, "Illegal TFTP operation");
            errorPacket.send(sockfd, clientAddr);
            break;
    }
    return;
}
