/**
 * @file server/main.cpp
 * @brief Entrypoint for TFTP server
 * @author Lukas Vecerka (xvecer30)
*/
#include <iostream>
#include <string>
#include <getopt.h>
#include "server/tftp_server.hpp"
#include "common/session.hpp"
#include "common/logger.hpp"
#include <csignal>

/**
 * Signal handler for SIGINT
 * @param signal The signal number
*/
void signalHandler(int signal) {
    if (signal == SIGINT){
        Logger::instance().log("Server is going to stop...");
        stopFlagServer->store(true);
    }
    
}

// Define the long options
static struct option long_options[] = {
    {"port", optional_argument, 0, 'p'},
    {0, 0, 0, 0} // End of array need to be filled with 0s
};

/**
 * Entrypoint for TFTP Server
 * @param argc The number of arguments
 * @param argv The arguments
 * @return 0 if successful, 1 otherwise
*/
int main(int argc, char* argv[]) {
    int port = 69;
    std::string root_dirpath;
    int option_index = 0;
    int option;

    while ((option = getopt_long(argc, argv, "p:", long_options, &option_index)) != -1) {
        switch (option) {
            case 'p':
                try{
                    port = std::stoi(optarg);
                } catch (const std::exception& e) {
                    Logger::instance().log("Invalid port number. Port should be between 1 and 65535.");
                    Logger ::instance().log("Usage: " + std::string(argv[0]) + " [-p port] root_dirpath");
                    return 1;
                }
                if (port <= 0 || port > 65535) {
                    Logger::instance().log("Invalid port number. Port should be between 1 and 65535.");
                    Logger ::instance().log("Usage: " + std::string(argv[0]) + " [-p port] root_dirpath");
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
        Logger::instance().log("Root directory path: " + root_dirpath);
    } else {
        Logger::instance().log("Root directory path is not specified.");
        Logger ::instance().log("Usage: " + std::string(argv[0]) + " [-p port] root_dirpath");
        return 1;
    }

    
    std::signal(SIGINT, signalHandler);
    // Initialize and start the TFTP server
    try {
        TFTPServer tftpServer(port, root_dirpath);
        tftpServer.start();
    } catch (const std::exception& e) {
        Logger::instance().log("Failed to start TFTP server: " + std::string(e.what()));
        return 1;
    }

    return 0;
}