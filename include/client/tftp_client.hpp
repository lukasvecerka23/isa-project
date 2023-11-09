// tftp_server.hpp
#ifndef TFTPCLIENT_HPP
#define TFTPCLIENT_HPP

#include <string>

class TFTPClient {
public:
    TFTPClient(std::string hostname, int port);
    void upload(std::string dest_filepath);
    void download(std::string filepath, std::string dest_filepath);

private:
    std::string hostname;
    int port;
};

#endif 