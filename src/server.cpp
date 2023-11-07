// server.cpp
#include <iostream>
#include <string>
#include <getopt.h>

// include other necessary headers

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
    } else {
        std::cerr << "Usage: " << argv[0] << " [-p port] root_dirpath\n";
        return 1;
    }

    // All arguments should be parsed at this point.
    std::cout << "Server starting on port: " << port << "\n"
              << "Root directory: " << root_dirpath << "\n";

    // Your TFTP server logic goes here.
    // You would set up a socket to listen on the given port and handle requests,
    // storing incoming files to the given root directory.

    return 0;
}