/**
 * @file common/session.hpp
 * @brief Header file with declaration for sessions
 * @author Lukas Vecerka (xvecer30)
*/
#ifndef SESSION_HPP
#define SESSION_HPP
#define BUFFER_SIZE 65507
#define MAX_BLOCK_SIZE 65464
#define MAX_TIMEOUT 255
#define MAX_TSIZE 4290183240 // 65464 * 65535
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

/**
 * @brief Flag for handling SIGINT on server
*/
extern std::shared_ptr<std::atomic<bool>> stopFlagServer;

/**
 * @brief Flag for handling SIGINT on client
*/
extern std::shared_ptr<std::atomic<bool>> stopFlagClient;

/**
 * @brief Enum for error codes
*/
enum ErrorCode {
    NOT_DEFINED = 0,
    FILE_NOT_FOUND = 1,
    ACCESS_VIOLATION = 2,
    DISK_FULL = 3,
    ILLEGAL_OPERATION = 4,
    UNKNOWN_TID = 5,
    FILE_ALREADY_EXISTS = 6,
    NO_SUCH_USER = 7,
    INVALID_OPTIONS = 8
};

/**
 * @brief Enum for opcodes
*/
enum Opcode{
    RRQ = 1,
    WRQ = 2,
    DATA = 3,
    ACK = 4,
    ERROR = 5,
    OACK = 6
};

/**
 * @brief Enum for data modes
*/
enum DataMode {
    NETASCII,
    OCTET
};

/**
 * @brief Enum for session types
*/
enum SessionType {
    READ,
    WRITE
};

/**
 * @brief Enum for session states
*/
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

/**
 * @brief Function for converting mode enum to string
 * @param value Mode to convert
 * @return mode as a string
*/
std::string modeToString(DataMode value);

/**
 * @brief Function for converting string to mode enum
 * @param value String to convert
 * @return DataMode
*/
DataMode stringToMode(std::string value);

/**
 * @brief Function for determine if there is enough space for file when tsize is presented in options
 * @param size Size of file
 * @param rootDir Root directory
 * @return true if there is enough space, false otherwise
*/
bool hasEnoughSpace(uint64_t size, std::string rootDir);


class Packet;

/**
 * @brief Class for representing Session
 * @note This class is base class for ClientSession and ServerSession
*/
class Session {
public:
    Session(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::string rootDir);
    /**
     * @brief Function for handling session
    */
    virtual void handleSession() {}
    sockaddr_in dst_addr;
    sockaddr_in src_addr;
    int srcTID;
    int sessionSockfd;
    uint16_t blockNumber;
    uint16_t blockSize;
    int timeout;
    int initialTimeout;
    uint64_t tsize;
    DataMode dataMode;
    SessionType sessionType;
    std::string rootDir;
    std::string src_filename;
    std::string dst_filename;
    SessionState sessionState;
    std::ofstream writeStream;
    bool fileOpen;
    std::map<std::string, uint64_t> options;
    int retries;
    std::unique_ptr<Packet> lastPacket;
    /**
     * @brief Function for writing data block to file
     * @param data Data to write
     * @throw std::runtime_error if failed to write into file
    */
    void writeDataBlock(std::vector<char> data);
    /**
     * @brief Function for setting timeout on socket
     * 
    */
    void setTimeout();
    /**
     * @brief Function for opening file for writing
     * @return true if file was opened, false otherwise
    */
    bool openFileForWrite();
};

class ClientSession : public Session {
public:
    bool TIDisSet;
    ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, uint64_t> options, std::string rootDir);
    void handleSession() override;
    /**
     * @brief Function for reading data block from stdin
     * @return vector of chars
     * @throw std::runtime_error if failed to read from stdin
    */
    std::vector<char> readDataBlock();
    /**
     * @brief Function for setting options on client when OACK is received
     * @param options Options to set
     * 
    */
    void setOptions(std::map<std::string, uint64_t> options);
    /**
     * @brief Function for cleaning the session
    */
    void exit();
};

class ServerSession : public Session {
public:
    std::ifstream readStream;
    ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType,  std::map<std::string, uint64_t> options, std::string rootDir);
    void handleSession() override;
    /**
     * @brief Function for reading data block from file
     * @return vector of 
     * @throw std::runtime_error if failed to read from file
    */
    std::vector<char> readDataBlock();
    /**
     * @brief Function for cleaning session
    */
    void exit();
    /**
     * @brief Function for setting options on server when DATA/ACK is received after OACK was sent
    */
    void setOptions();
    /**
     * @brief Function for handling write request
     * @return true if write request was handled, false otherwise
    */
    bool handleWriteRequest();
    /**
     * @brief Function for handling read request
     * @return true if read request was handled, false otherwise
    */
    bool handleReadRequest();
    /**
     * @brief Function for opening file for reading
     * @return true if file was opened, false otherwise
    */
    bool openFileForRead();
};

#endif 