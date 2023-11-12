// tftp_server.cpp
#include "server/tftp_server.hpp"
#include "common/packets.hpp"
#include "common/session.hpp"
#include <sys/stat.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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

TFTPServer::TFTPServer(int port, std::string rootDirPath)
    : port(port), rootDirPath(rootDirPath) {
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

        struct stat st = {0};

        if (stat(rootDirPath.c_str(), &st) == -1) {
            // Directory does not exist, attempt to create it
            if (mkdir(rootDirPath.c_str(), 0700) == -1) { // 0700 permissions - owner can read, write, and execute
                std::cerr << "Failed to create directory: " << rootDirPath << std::endl;
            }
        }
        std::cout << "Starting TFTP server on port " << port 
        << " with root directory: " << rootDirPath << std::endl;
    }

void TFTPServer::start() {
    std::cout << "Server listening on port " << port << std::endl;

    // Enter a loop to receive data
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        char buffer[2048]; // A buffer to store incoming data
        // Receive initial request from a clients
        ssize_t received_bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                          (struct sockaddr *)&client_addr, &client_len);
        if (received_bytes < 0) {
            std::cout << "Failed to receive data\n";
            continue; // In a real server you might want to handle this differently
        }

        // TODO: run new thread for each client
        // Handle the received data
        handleClientRequest(client_addr, buffer, received_bytes);
    }

    // Close the socket (this will never be reached in this example, but should be done when the server stops)
    close(sockfd);
}

void TFTPServer::handleClientRequest(const sockaddr_in& clientAddr, const char* buffer, ssize_t bufferSize) {
    // Parse the packet
    try {
        std::unique_ptr<Packet> packet = Packet::parse(clientAddr, buffer, bufferSize);

        // Handle the packet
        switch (packet->getOpcode()){
            case 1: // RRQ
            {
                int sockfd = bind_new_socket();
                ReadRequestPacket* readPacket = dynamic_cast<ReadRequestPacket*>(packet.get());
                ServerSession readSession(sockfd, clientAddr, rootDirPath + "/" + readPacket->filename, "", readPacket->mode == "netascii" ? DataMode::NETASCII : DataMode::OCTET, SessionType::WRITE);
                readSession.handleSession();
                break;
            }
            case 2: // WRQ
            {
                int sockfd = bind_new_socket();
                WriteRequestPacket* writePacket = dynamic_cast<WriteRequestPacket*>(packet.get());
                ServerSession writeSession(sockfd, clientAddr, "", rootDirPath + "/" + writePacket->filename, writePacket->mode == "netascii" ? DataMode::NETASCII : DataMode::OCTET, SessionType::WRITE);
                writeSession.handleSession();
                break;
            }
            default:
                throw std::runtime_error("Unknown or unhandled TFTP opcode");
        }

    } catch (const std::runtime_error& e) {
        std::cerr << "Failed to parse packet: " << e.what() << std::endl;
        // Handle error
    }
}

