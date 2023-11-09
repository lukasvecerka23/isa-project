// tftp_server.cpp
#include "server/tftp_server.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

TFTPServer::TFTPServer(int port, std::string rootDirPath)
    : port(port), rootDirPath(std::move(rootDirPath)) {}

void TFTPServer::start() {
    std::cout << "Starting TFTP server on port " << port 
              << " with root directory: " << rootDirPath << std::endl;

        // Create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Cannot open socket\n";
        return;
    }

    // Initialize server address structure
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
    server_addr.sin_port = htons(port);

    // Bind socket to the server address
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Cannot bind socket to port " << port << "\n";
        close(sockfd);
        return;
    }

    std::cout << "Server listening on port " << port << std::endl;

    // Enter a loop to receive data
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        char buffer[2048]; // A buffer to store incoming data

        // Receive data from a client
        ssize_t received_bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                          (struct sockaddr *)&client_addr, &client_len);
        if (received_bytes < 0) {
            std::cerr << "Failed to receive data\n";
            continue; // In a real server you might want to handle this differently
        }

        // Print the received data to stdout for demonstration purposes
        std::cout << "Received data: " << std::string(buffer, received_bytes) << std::endl;

        // Here you would normally process the received data and respond accordingly
        // For now, just echo the data back to the sender for testing purposes
        sendto(sockfd, buffer, received_bytes, 0, (struct sockaddr *)&client_addr, client_len);
    }

    // Close the socket (this will never be reached in this example, but should be done when the server stops)
    close(sockfd);
}