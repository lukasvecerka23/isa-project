// tftp_server.hpp
#ifndef SESSION_HPP
#define SESSION_HPP
#define BUFFER_SIZE 65507
#define MAX_BLOCK_SIZE 65464
#define MAX_TIMEOUT 255
#define MAX_TSIZE 4294967295
#define MIN_BLOCK_SIZE 8
#define MIN_TIMEOUT 1
#define MIN_TSIZE 0
#define INITIAL_TIMEOUT 5
#define INITIAL_BLOCK_SIZE 512
#define INITIAL_TSIZE 0
#define MAX_RETRIES 3
#define BACKOFF_FACTOR 2


#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fstream>
#include <map>

extern std::shared_ptr<std::atomic<bool>> stopFlagServer;
extern std::shared_ptr<std::atomic<bool>> stopFlagClient;

enum class DataMode {
    NETASCII,
    OCTET
};

std::string modeToString(DataMode value);

DataMode stringToMode(std::string value);

bool hasEnoughSpace(uint64_t size);

enum class SessionType {
    READ,
    WRITE
};

enum class SessionState {
    INITIAL,
    WAITING_OACK,
    WAITING_AFTER_OACK,
    WAITING_ACK,
    WAITING_LAST_ACK,
    WAITING_DATA,
    WRQ_END,
    RRQ_END,
    ERROR
};

class Packet;

class Session {
public:
    Session(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType);
    virtual void handleSession() {}
    sockaddr_in dst_addr;
    int srcTID;
    int sessionSockfd;
    uint16_t blockNumber;
    uint16_t blockSize;
    int timeout;
    uint64_t tsize;
    DataMode dataMode;
    SessionType sessionType;
    std::string src_filename;
    std::string dst_filename;
    SessionState sessionState;
    std::ofstream writeStream;
    bool fileOpen;
    std::map<std::string, uint64_t> options;
    int retries;
    std::unique_ptr<Packet> lastPacket;
    void writeDataBlock(std::vector<char> data);
    void setTimeout();
    bool openFileForWrite();
};

class ClientSession : public Session {
public:
    ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, uint64_t> options);
    void handleSession() override;
    std::vector<char> readDataBlock();
    void setOptions(std::map<std::string, uint64_t> options);
    bool TIDisSet;
    void exit();
};

class ServerSession : public Session {
public:
    ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType,  std::map<std::string, uint64_t> options);
    void handleSession() override;
    std::vector<char> readDataBlock();
    std::ifstream readStream;
    void exit();
    void setOptions();
    bool handleWriteRequest();
    bool handleReadRequest();
    bool openFileForRead();
};

#endif 