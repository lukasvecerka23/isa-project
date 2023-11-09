// tftp_server.cpp
#include "server/tftp_server.hpp"
#include <iostream>

TFTPServer::TFTPServer(int port, std::string rootDirPath)
    : port(port), rootDirPath(std::move(rootDirPath)) {}

void TFTPServer::start() {
    std::cout << "Starting TFTP server on port " << port 
              << " with root directory: " << rootDirPath << std::endl;

    // Code to bind the socket and listen for incoming requests...

    // ... and then entering the main loop to handle those requests.
}