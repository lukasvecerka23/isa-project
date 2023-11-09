// tftp_server.hpp
#ifndef TFTPSERVER_HPP
#define TFTPSERVER_HPP

#include <string>

class TFTPServer {
public:
    TFTPServer(int port, std::string rootDirPath);
    void start();

private:
    int port;
    std::string rootDirPath;
};

#endif 