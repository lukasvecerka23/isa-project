// tftp_server.hpp
#ifndef TFTPSERVER_HPP
#define TFTPSERVER_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread> 
#include <sys/stat.h>
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

class TFTPServer {
public:
    static TFTPServer& getInstance(){
        static TFTPServer instance;
        return instance;
    }

    static void initialize(int port, const std::string& rootDirPath) {
        TFTPServer& instance = getInstance();
        instance.port = port;
        instance.rootDirPath = rootDirPath;
        // Create socket
        instance.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (instance.sockfd < 0) {
            throw std::runtime_error("Failed to open socket");
        }
        // Initialize server address structure
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
        server_addr.sin_port = htons(port);

        // Bind socket to the server address
        if (bind(instance.sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            close(instance.sockfd);
            throw std::runtime_error("Failed to bind socket to port");
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        if (setsockopt(instance.sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
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
    
    TFTPServer(TFTPServer const&) = delete;
    TFTPServer& operator=(TFTPServer const&) = delete;

    void start();
    void shutDown();

private:
    TFTPServer() = default;
    static TFTPServer* server_;
    int port;
    std::string rootDirPath;
    int sockfd;
    void handleClientRequest(const sockaddr_in& clientAddr, const char* buffer, ssize_t bufferSize);
    std::vector<std::future<void>> clientFutures;
};

#endif 