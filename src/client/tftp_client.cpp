// tftp_server.cpp
#include "client/tftp_client.hpp"
#include <iostream>

TFTPClient::TFTPClient(std::string hostname, int port)
    : hostname(std::move(hostname)), port(port) {}

void TFTPClient::upload(std::string dest_filepath) {
    std::cout << "Uploading file to " << hostname << ":" << port 
              << " with destination filepath: " << dest_filepath << std::endl;

    // Code to bind the socket and listen for incoming requests...

    // ... and then entering the main loop to handle those requests.
}

void TFTPClient::download(std::string filepath, std::string dest_filepath) {
    std::cout << "Downloading file from " << hostname << ":" << port 
              << " with filepath: " << filepath 
              << " to destination filepath: " << dest_filepath << std::endl;

    // Code to bind the socket and listen for incoming requests...

    // ... and then entering the main loop to handle those requests.
}