// tftp_server.hpp
#ifndef SESSION_HPP
#define SESSION_HPP

#include <string>
#include <netinet/in.h>
#include <sys/socket.h>

enum class DataMode {
    NETASCII,
    OCTET
};

enum class SessionType {
    READ,
    WRITE
};

enum class SessionState {
    INITIAL,
    WAITING_ACK,
    WAITING_DATA,
    END
};

class Session {
public:
    Session(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType);
    ~Session();
    virtual void handleSession() {}
    sockaddr_in dst_addr;
    int srcTID;
    int sessionSockfd;
    int blockNumber;
    int blockSize;
    DataMode dataMode;
    SessionType sessionType;
    std::string src_filename;
    std::string dst_filename;
    SessionState sessionState;
};

class ClientSession : public Session {
public:
    ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType);
    void handleSession() override;
};

class ServerSession : public Session {
public:
    ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType);
    void handleSession() override;
};

#endif 