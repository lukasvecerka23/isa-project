/**
 * @file client/main.cpp
 * @brief Entrypoint for TFTP client
 * @author Lukas Vecerka (xvecer30)
*/
#include <iostream>
#include <string>
#include <getopt.h>
#include "client/tftp_client.hpp"
#include <csignal>
#include "common/logger.hpp"
// include other necessary headers

void signalHandler(int signal) {
    Logger::instance().log("Client is going to stop...");
    if (signal == SIGINT){
        stopFlagClient->store(true);
    }
    
}

// Define the long options
static struct option long_options[] = {
    {"hostname", required_argument, 0, 'h'},
    {"file", optional_argument, 0, 'f'},
    {"dest", required_argument, 0, 't'},
    {"port", optional_argument, 0, 'p'},
    {0, 0, 0, 0} // End of array need to be filled with 0s
};

int main(int argc, char* argv[]) {
    std::string hostname;
    int port = 69; // Default TFTP port
    std::string filepath;
    std::string dest_filepath;
    bool upload = true;
    int option_index = 0;
    int option;

    while ((option = getopt_long(argc, argv, "h:p:f:t:", long_options, &option_index)) != -1) {
        switch (option) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                try{
                    port = std::stoi(optarg);
                } catch (const std::exception& e) {
                    Logger::instance().log("Invalid port number. Port should be between 1 and 65535.");
                    Logger::instance().log("Usage: " + std::string(argv[0]) + " -h hostname [-p port] [-f filepath] -t dest_filepath");
                    return 1;
                }
                
                if (port <= 0 || port > 65535) {
                    Logger::instance().log("Invalid port number. Port should be between 1 and 65535.");
                    Logger::instance().log("Usage: " + std::string(argv[0]) + " -h hostname [-p port] [-f filepath] -t dest_filepath");
                    return 1;
                }
                break;
            case 'f':
                filepath = optarg;
                upload = false;
                break;
            case 't':
                dest_filepath = optarg;
                break;
            case '?': // Option not recognized
                return 1;
            default:
                break;
        }
    }

    if (hostname.empty() || dest_filepath.empty() || (!upload && filepath.empty())) {
        Logger::instance().log("Missing required arguments.");
        Logger::instance().log("Usage: " + std::string(argv[0]) + " -h hostname [-p port] [-f filepath] -t dest_filepath");
        return 1;
    }

    std::signal(SIGINT, signalHandler);

    try {
        TFTPClient client(hostname, port); // Create an instance of the TFTPClient with the given host and port
        
        // Check the operation mode based on the presence of the filepath
        if (upload) {
            // If no filepath is provided, assume data will come from stdin.
            client.upload(dest_filepath);
        } else {
            // Otherwise, download the file from the given filepath.
            client.download(filepath, dest_filepath);
        }
    } catch (const std::exception& e) {
        Logger::instance().log("Failed to start TFTP client: " + std::string(e.what()));
        return 1;
    }

    return 0;
}