/**
 * @file client/tftp_client.hpp
 * @brief Header file with declaration for TFTP client
 * @author Lukas Vecerka (xvecer30)
*/
#ifndef TFTPCLIENT_HPP
#define TFTPCLIENT_HPP
#define BUFFER_SIZE 65507

#include <string>
#include "common/session.hpp"

/**
 * @class TFTPClient
*/
class TFTPClient {
public:
    TFTPClient(std::string hostname, int port);
    /**
     * @brief Function for sending WRQ packet to server and handle uploading of file
     * @param dest_filepath The destination filepath on server
    */
    void upload(std::string dest_filepath);
    /**
     * @brief Function for sending RRQ packet to server and handle downloading of file
     * @param filepath The filepath on server
     * @param dest_filepath The destination filepath on client
    */
    void download(std::string filepath, std::string dest_filepath);

private:
    std::string hostname;
    int port;
    int sockfd;
};

#endif 