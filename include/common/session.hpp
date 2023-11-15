// tftp_server.hpp
#ifndef SESSION_HPP
#define SESSION_HPP
#define BUFFER_SIZE 65507

#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fstream>
#include <map>

enum class DataMode {
    NETASCII,
    OCTET
};

std::string modeToString(DataMode value);

DataMode stringToMode(std::string value);

enum class SessionType {
    READ,
    WRITE
};

enum class SessionState {
    INITIAL,
    WAITING_OACK,
    WAITING_ACK,
    WAITING_LAST_ACK,
    WAITING_DATA,
    WRQ_END,
    RRQ_END
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
    int timeout;
    int tsize;
    DataMode dataMode;
    SessionType sessionType;
    std::string src_filename;
    std::string dst_filename;
    SessionState sessionState;
    std::ofstream writeStream;
    std::map<std::string, int> options;
    void writeDataBlock(std::vector<char> data);
};

class ClientSession : public Session {
public:
    ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, int> options);
    void handleSession() override;
    std::vector<char> readDataBlock();
    void setOptions(std::map<std::string, int> options);
};

class ServerSession : public Session {
public:
    ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType,  std::map<std::string, int> options);
    void handleSession() override;
    std::vector<char> readDataBlock();
    std::ifstream readStream;
    
};

#endif 