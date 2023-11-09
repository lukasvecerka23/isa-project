// server.cpp
#include <iostream>
#include <string>
#include <getopt.h>
#include "client/tftp_client.hpp"
// include other necessary headers

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
                port = std::stoi(optarg);
                if (port <= 0) {
                    std::cerr << "Invalid port number.\n";
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

    if (hostname.empty() || dest_filepath.empty()) {
        std::cerr << "Usage: " << argv[0] << " -h hostname [-p port] [-f filepath] -t dest_filepath\n";
        return 1;
    }

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
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}