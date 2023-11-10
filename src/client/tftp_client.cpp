// tftp_server.cpp
#include "client/tftp_client.hpp"
#include "common/packets.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

TFTPClient::TFTPClient(std::string hostname, int port)
    : hostname(std::move(hostname)), port(port) {}

void TFTPClient::upload(std::string dest_filepath) {
    std::cout << "Uploading file to " << hostname << ":" << port 
              << " with destination filepath: " << dest_filepath << std::endl;

        // Create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Cannot open socket\n";
        return;
    }

    // Initialize hints for getaddrinfo
    struct addrinfo hints, *res;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // AF_INET for IPv4
    hints.ai_socktype = SOCK_DGRAM; // Datagram socket for UDP

    // Resolve hostname to IP address
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0) {
        std::cerr << "Could not resolve hostname\n";
        close(sockfd);
        return;
    }

    // Copy the resolved address to our server address structure
    struct sockaddr_in server_addr = *(struct sockaddr_in*)res->ai_addr;
    server_addr.sin_port = htons(port);

    TFTPErrorPacket packet(5, "Zde je error"); // Create an ACK packet with block number 0
    std::vector<char> message = packet.serialize();

    // Send data to server
    if (sendto(sockfd, message.data(), message.size(), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Sendto failed\n";
        close(sockfd);
        return;
    }

    std::cout << "Message sent to server successfully\n";

    // Here you might want to wait for a response depending on your application's needs

    // Close the socket
    close(sockfd);
}

void TFTPClient::download(std::string filepath, std::string dest_filepath) {
    std::cout << "Downloading file from " << hostname << ":" << port 
              << " with filepath: " << filepath 
              << " to destination filepath: " << dest_filepath << std::endl;

    // Code to bind the socket and listen for incoming requests...

    // ... and then entering the main loop to handle those requests.
}