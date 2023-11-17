// tftp_server.cpp
#include "client/tftp_client.hpp"
#include "common/packets.hpp"
#include "common/session.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

TFTPClient::TFTPClient(std::string hostname, int port)
    : hostname(std::move(hostname)), port(port) {
        // Create socket
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to open socket");
        }

        // Initialize server address structure
        sockaddr_in client_addr{};
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
        client_addr.sin_port = htons(0); // Let OS choose the port

        // Bind socket to the server address
        if (bind(sockfd, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            close(sockfd);
            throw std::runtime_error("Failed to bind socket to port");
        }
    }

void TFTPClient::upload(std::string dest_filepath) {
    std::cout << "Uploading file to " << hostname << ":" << port 
              << " with destination filepath: " << dest_filepath << std::endl;

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

    // Send the initial request to the server
    struct sockaddr_in server_addr = *(struct sockaddr_in*)res->ai_addr;
    server_addr.sin_port = htons(port);

    std::map<std::string, uint64_t> options;
    options["blksize"] = 20;

    std::ostringstream ss;
    ss << std::cin.rdbuf();
    std::string stdinData = ss.str();
    options["tsize"] = stdinData.size();

    std::istringstream iss(stdinData);
    std::cin.rdbuf(iss.rdbuf());

    WriteRequestPacket packet(dest_filepath, DataMode::OCTET, options); // Create an ACK packet with block number 0
    packet.send(sockfd, server_addr);


    struct sockaddr_in from_addr;

    ClientSession session(sockfd, from_addr, "stdin", dest_filepath, DataMode::OCTET, SessionType::WRITE, options);
    session.handleSession();
}

void TFTPClient::download(std::string filepath, std::string dest_filepath) {
    std::cout << "Downloading file from " << hostname << ":" << port 
              << " with filepath: " << filepath 
              << " to destination filepath: " << dest_filepath << std::endl;

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

    // Send the initial request to the server
    struct sockaddr_in server_addr = *(struct sockaddr_in*)res->ai_addr;
    server_addr.sin_port = htons(port);

    std::map<std::string, uint64_t> options;
    options["blksize"] = 20;
    options["tsize"] = 0; // 0 means we don't know the size of the file yet

    ReadRequestPacket packet(filepath, DataMode::OCTET, options);
    packet.send(sockfd, server_addr);

    struct sockaddr_in from_addr;

    ClientSession session(sockfd, from_addr, filepath, dest_filepath, DataMode::OCTET, SessionType::READ, options);
    session.handleSession();
}