// tftp_server.hpp
#ifndef TFTPCLIENT_HPP
#define TFTPCLIENT_HPP
#define BUFFER_SIZE 65507

#include <string>

class TFTPClient {
public:
    TFTPClient(std::string hostname, int port);
    void upload(std::string dest_filepath);
    void download(std::string filepath, std::string dest_filepath);

private:
    std::string hostname;
    int port;
    int sockfd;
};

#endif 