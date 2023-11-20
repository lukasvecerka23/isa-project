/**
 * @file common/packets.hpp
 * @brief Header file with declaration for packets
 * @author Lukas Vecerka (xvecer30)
*/

#ifndef PACKETS_HPP
#define PACKETS_HPP

#include <vector>
#include <map>
#include <set>
#include <string>
#include <netinet/in.h>
#include "common/session.hpp"

/**
 * Function for convert netascii from packet to normal string
 * @param buffer The buffer to read from
 * @param start The start of the string
 * @param end The end of the string
 * @return A pair of the string and the next position in the buffer
*/
std::pair<std::string, const char*> parseNetasciiString(const char* buffer, const char* start, const char* end);

/**
 * @class Packet
 * @brief This class is an abstract base class for all packet classes, declare vertiual functions which should be implemented
 * by all packet classes
*/
class Packet {
public:
    sockaddr_in addr;
    virtual ~Packet() = default;
    /**
     * @brief Function for serialize packet to vector of char before sending
     * @return Vector of char
    */
    virtual std::vector<char> serialize() const = 0;
    /**
     * @brief Function to get opcode of packet
    */
    virtual Opcode getOpcode() const = 0;
    /**
     * @brief Function to handle packet logic on client side
     * @param session The session to handle
    */
    virtual void handleClient(ClientSession* session) const {}
    /**
     * @brief Function to handle packet logic on server side
     * @param session The session to handle
    */
    virtual void handleServer(ServerSession* session) const {}
    /**
     * @brief Function which retruns unique pointer on pocket based on opcode of the packet
     * @param addr The address of source
     * @param buffer The buffer received from socket
     * @param bufferSize The size of buffer
     * @return Unique pointer to packet
     * @throws ParsingError if packet is not valid, OptionError if option is not valid
    */
    static std::unique_ptr<Packet> parse(sockaddr_in addr, const char* buffer, size_t bufferSize);
    void send(Session* session, int socket);
};

/**
 * @brief Class for represent RRQ/WRQ packets
 * @note This class inherits from Packet class and implements all its methods
 * @note filename - path to file on server
 * @note mode - mode of transfer (netascii, octet)
 * @note options - map of options
 * @note supportedOptions - set of supported options
*/
class RequestPacket : public Packet {
public:
    std::string filename;
    DataMode mode;
    std::map<std::string, uint64_t> options;
    static const std::set<std::string> supportedOptions;
    RequestPacket(const std::string& filename, DataMode mode, std::map<std::string, uint64_t> options, sockaddr_in addr);
    std::vector<char> serialize() const override;
    static std::unique_ptr<RequestPacket> parse(sockaddr_in addr, const char* buffer, size_t size);
};

/**
 * @brief Class for represent RRQ packets
*/
class ReadRequestPacket : public RequestPacket {
public:
    ReadRequestPacket(const std::string& filename, DataMode mode, std::map<std::string, uint64_t> options, sockaddr_in addr);
    Opcode getOpcode() const override { return Opcode::RRQ; }
    void handleClient(ClientSession* session) const override;
    void handleServer(ServerSession* session) const override;
};

/**
 * @brief Class for represent WRQ packets
*/
class WriteRequestPacket : public RequestPacket {
public:
    WriteRequestPacket(const std::string& filename, DataMode mode, std::map<std::string, uint64_t> options, sockaddr_in addr);
    Opcode getOpcode() const override { return Opcode::WRQ; }
    void handleClient(ClientSession* session) const override;
    void handleServer(ServerSession* session) const override;
};

/**
 * @brief Class for represent DATA packets
 * @note This class inherits from Packet class and implements all its methods
 * @note blockNumber - number of block
 * @note data - vector of data
*/
class DataPacket : public Packet {
public:
    uint16_t blockNumber;
    std::vector<char> data;
    DataPacket(uint16_t blockNumber, const std::vector<char>& data, sockaddr_in addr);
    std::vector<char> serialize() const override;
    static DataPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    Opcode getOpcode() const override { return Opcode::DATA; } // DATA opcode
    void handleClient(ClientSession* session) const override;
    void handleServer(ServerSession* session) const override;
};

/**
 * @brief Class for represent ACK packets
 * @note This class inherits from Packet class and implements all its methods
 * @note blockNumber - number of block
*/
class ACKPacket : public Packet {
public:
    uint16_t blockNumber;
    ACKPacket(uint16_t blockNumber, sockaddr_in addr);
    std::vector<char> serialize() const override;
    static ACKPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    Opcode getOpcode() const override { return Opcode::ACK; } // ACK opcode
    void handleClient(ClientSession* session) const override;
    void handleServer(ServerSession* session) const override;
};

/**
 * @brief Class for represent ERROR packets
 * @note This class inherits from Packet class and implements all its methods
 * @note errorCode - error code
 * @note errorMessage - error message
*/
class ErrorPacket : public Packet {
public:
    ErrorCode errorCode;
    std::string errorMessage;
    ErrorPacket(ErrorCode errorCode, const std::string& errorMessage, sockaddr_in addr);
    std::vector<char> serialize() const override;
    static ErrorPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    Opcode getOpcode() const override { return Opcode::ERROR; } // ERROR opcode
    void handleClient(ClientSession* session) const override;
    void handleServer(ServerSession* session) const override;
};

/**
 * @brief Class for represent OACK packets
 * @note This class inherits from Packet class and implements all its methods
 * @note options - map of options
*/
class OACKPacket : public Packet {
public:
    std::map<std::string, uint64_t> options;
    OACKPacket(std::map<std::string, uint64_t> options, sockaddr_in addr);
    std::vector<char> serialize() const override;
    static OACKPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    Opcode getOpcode() const override { return Opcode::OACK; } // OACK opcode
    void handleClient(ClientSession* session) const override;
    void handleServer(ServerSession* session) const override;
};

#endif // PACKETS_HPP