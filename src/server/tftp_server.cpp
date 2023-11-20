/**
 * @file server/tftp_server.cpp
 * @brief Implementation of TFTP Server
 * @author Lukas Vecerka (xvecer30)
*/
#include "server/tftp_server.hpp"
#include "common/packets.hpp"
#include "common/session.hpp"
#include "common/exceptions.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <future>

/**
 * @brief Function for creating new socket and bind it to new address and set initial timeout
*/
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
    server_addr.sin_port = htons(0); // Let OS choose the port

    // Bind socket to new address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to bind socket to port");
    }

    // Set initial timeout
    struct timeval tv;
    tv.tv_sec = INITIAL_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        Logger::instance().log("Error setting socket options: " + std::string(strerror(errno)));
    }

    return sockfd;
}

void TFTPServer::shutDown() {
    // Wait for all client threads to finish
    for (auto& future : clientFutures) {
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::timeout) {
            Logger::instance().log("Waiting for client session to terminate...");
            future.get(); // This will block until the future is finished
            Logger::instance().log("Client session terminated");
        }
    }

    // Clear the vector of futures
    clientFutures.clear();

    close(sockfd);

    return;
}

TFTPServer::TFTPServer(int port, const std::string& rootDirPath){
        this->port = port;
        this->rootDirPath = rootDirPath;
        // Create socket
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to open socket");
        }
        // Initialize server address structure
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
        server_addr.sin_port = htons(port);

        // Bind socket to the server address
        if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            close(sockfd);
            throw std::runtime_error("Failed to bind socket to port");
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            std::cout << "Error setting socket options: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Socket timeout set" << std::endl;
        }

        struct stat st = {0};

        if (stat(rootDirPath.c_str(), &st) == -1) {
            // Directory does not exist, attempt to create it
            if (mkdir(rootDirPath.c_str(), 0700) == -1) { // 0700 permissions - owner can read, write, and execute
                std::cout << "Failed to create directory: " << rootDirPath << std::endl;
                throw std::runtime_error("Failed to create directory");
            }
        }
        std::cout << "Starting TFTP server on port " << port 
        << " with root directory: " << rootDirPath << std::endl;
}

void TFTPServer::start() {
    Logger::instance().log("Server listening on port " + std::to_string(port));

    // Main loop of TFTP Server which is receiving requests from clients
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    while (true) {
        // Receive initial request from a clients
        ssize_t received_bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                          (struct sockaddr *)&client_addr, &client_len);
        if (received_bytes < 0) {
            // If SIGINT was received, stop the server
            if (stopFlagServer->load()){
                Logger::instance().log("Stopping server...");
                shutDown();
               return;
            }
            continue;
        }

        // Create new feature with handleClientRequest
        auto future = std::async(std::launch::async, &TFTPServer::handleClientRequest, this, client_addr, buffer, received_bytes);
        clientFutures.push_back(std::move(future));

        // Remove finished futures
        clientFutures.erase(std::remove_if(clientFutures.begin(), clientFutures.end(), [](const std::future<void>& f) {
            return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }), clientFutures.end());
    }
}

void TFTPServer::handleClientRequest(const sockaddr_in& clientAddr, const char* buffer, ssize_t bufferSize) {
    // Parse the first packet
    std::unique_ptr<Packet> packet;
    try {
        packet = Packet::parse(clientAddr, buffer, bufferSize);
    }
    catch (const ParsingError& e) {
        ErrorCode err = static_cast<ErrorCode>(ParsingError::errorCode);
        ErrorPacket errorPacket(err, e.what(), clientAddr);
        errorPacket.send(nullptr, sockfd);
        return;
    }
    catch (const OptionError& e) {
        ErrorCode err = static_cast<ErrorCode>(OptionError::errorCode);
        ErrorPacket errorPacket(err, e.what(), clientAddr);
        errorPacket.send(nullptr, sockfd);
        return;
    }
    catch (const std::exception& e) {
        ErrorPacket errorPacket(ErrorCode::NOT_DEFINED, e.what(), clientAddr);
        errorPacket.send(nullptr, sockfd);
        return;
    }

    // Handle the packet
    switch (packet->getOpcode()){
        case Opcode::RRQ: // RRQ
        {
            int sockfd = bind_new_socket();
            ReadRequestPacket* readPacket = dynamic_cast<ReadRequestPacket*>(packet.get());
            readPacket->filename = rootDirPath + "/" + readPacket->filename;
            if (!std::filesystem::exists(readPacket->filename)){
                ErrorPacket errorPacket(ErrorCode::FILE_NOT_FOUND, "File not found", clientAddr);
                errorPacket.send(nullptr, sockfd);
                close(sockfd);
                break;
            }
            ServerSession readSession(sockfd, clientAddr, readPacket->filename, "", readPacket->mode, SessionType::READ, readPacket->options, rootDirPath);
            readSession.handleSession();
            break;
        }
        case Opcode::WRQ: // WRQ
        {
            int sockfd = bind_new_socket();
            WriteRequestPacket* writePacket = dynamic_cast<WriteRequestPacket*>(packet.get());
            writePacket->filename = rootDirPath + "/" + writePacket->filename;
            if (std::filesystem::exists(writePacket->filename)){
                ErrorPacket errorPacket(ErrorCode::FILE_ALREADY_EXISTS, "File already exists", clientAddr);
                errorPacket.send(nullptr, sockfd);
                close(sockfd);
                break;
            }
            ServerSession writeSession(sockfd, clientAddr, "", writePacket->filename, writePacket->mode, SessionType::WRITE, writePacket->options, rootDirPath);
            writeSession.handleSession();
            break;
        }
        default:
            ErrorPacket errorPacket(ErrorCode::ILLEGAL_OPERATION, "Illegal TFTP operation", clientAddr);
            errorPacket.send(nullptr, sockfd);
            break;
    }
    return;
}
