// tftp_server.hpp
#ifndef TFTPSERVER_HPP
#define TFTPSERVER_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class TFTPServer {
public:
    TFTPServer(int port, std::string rootDirPath);
    void start();

private:
    int port;
    std::string rootDirPath;
    int sockfd;
    void handleClientRequest(const sockaddr_in& clientAddr, const char* buffer, ssize_t bufferSize);
};

#endif 