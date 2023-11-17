// server.cpp
#include <iostream>
#include <string>
#include <getopt.h>
#include "server/tftp_server.hpp"
#include "common/session.hpp"
#include <csignal>

// include other necessary headers

void signalHandler(int signal) {
    std::cout << "Stopping server..." << std::endl;
    if (signal == SIGINT){
        stopFlag->store(true);
        TFTPServer& tftpServer = TFTPServer::getInstance();
        tftpServer.shutDown();
    }
    
}

// Define the long options
static struct option long_options[] = {
    {"port", optional_argument, 0, 'p'},
    {0, 0, 0, 0} // End of array need to be filled with 0s
};

int main(int argc, char* argv[]) {
    int port = 69;
    std::string root_dirpath;
    int option_index = 0;
    int option;

    while ((option = getopt_long(argc, argv, "p:", long_options, &option_index)) != -1) {
        switch (option) {
            case 'p':
                port = std::stoi(optarg);
                if (port <= 0 || port > 65535) {
                    std::cerr << "Invalid port number.\n";
                    return 1;
                }
                break;
            case '?': // Option not recognized
                return 1;
            default:
                break;
        }
    }

    // The root directory path is not optional and should not be a flag-based argument
    // It should be the last argument after the options
    if (optind < argc) {
        root_dirpath = argv[optind];
        std::cout << "Root directory path: " << root_dirpath << std::endl;
    } else {
        std::cerr << "Usage: " << argv[0] << " [-p port] root_dirpath\n";
        return 1;
    }

    
    std::signal(SIGINT, signalHandler);
    // Initialize and start the TFTP server
    try {
        TFTPServer::initialize(port, root_dirpath);
        TFTPServer& tftpServer = TFTPServer::getInstance();
        tftpServer.start();
    } catch (const std::exception& e) {
        std::cerr << "Failed to start TFTP server: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}