/**
 * @file server/tftp_server.hpp
 * @brief Header file with declaration for TFTP server class and its methods and attributes
 * @author Lukas Vecerka (xvecer30)
*/
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

/**
 * @class TFTPServer
*/
class TFTPServer {
public:
    /**
     * @brief TFTPServer constructor which bind socket on port and create root directory
     * @param port The port to listen on
     * @param rootDirPath The root directory path
     * 
    */
    TFTPServer(int port, const std::string& rootDirPath);
    /**
     * @brief method for start main loop of server and receive new clients
    */
    void start();
    /**
     * @brief method for shutdown server when SIGINT is received
     * Method will gracefully exit all clients sessions and clean resources
    */
    void shutDown();

private:
    int port;
    std::string rootDirPath;
    int sockfd;
    /**
     * @brief method to handle new request packet from client, if request is valid it starts new client session
     * @param clientAddr The address of client
     * @param buffer The buffer received from socket
     * @param bufferSize The size of buffer
     * 
    */
    void handleClientRequest(const sockaddr_in& clientAddr, const char* buffer, ssize_t bufferSize);
    std::vector<std::future<void>> clientFutures;
};

#endif 