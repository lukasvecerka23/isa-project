#ifndef PACKETS_HPP
#define PACKETS_HPP

#include <vector>
#include <map>
#include <set>
#include <string>
#include <netinet/in.h>
#include "common/session.hpp"

std::string convertToNetascii(const std::string& str);
std::pair<std::string, const char*> parseNetasciiString(const char* buffer, const char* start, const char* end);

class Packet {
public:
    virtual ~Packet() = default;
    virtual std::vector<char> serialize() const = 0;
    virtual uint16_t getOpcode() const = 0;
    virtual void handleClient(ClientSession& session) const {}
    virtual void handleServer(ServerSession& session) const {}
    sockaddr_in addr;
    
    static std::unique_ptr<Packet> parse(sockaddr_in addr, const char* buffer, size_t bufferSize);
    void send(int socket, sockaddr_in dst_addr);
};

class RequestPacket : public Packet {
public:
    std::string filename;
    DataMode mode;
    std::map<std::string, int> options;
    static const std::set<std::string> supportedOptions;
    RequestPacket(const std::string& filename, DataMode mode, std::map<std::string, int> options);
    std::vector<char> serialize() const override;
};

class ReadRequestPacket : public RequestPacket {
public:
    ReadRequestPacket(const std::string& filename, DataMode mode, std::map<std::string, int> options);
    uint16_t getOpcode() const override { return 1; } // RRQ opcode
    static ReadRequestPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    void handleClient(ClientSession& session) const override;
    void handleServer(ServerSession& session) const override;
};

class WriteRequestPacket : public RequestPacket {
public:
    WriteRequestPacket(const std::string& filename, DataMode mode, std::map<std::string, int> options);
    uint16_t getOpcode() const override { return 2; } // WRQ opcode
    static WriteRequestPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    void handleClient(ClientSession& session) const override;
    void handleServer(ServerSession& session) const override;
};

class DataPacket : public Packet {
public:
    uint16_t blockNumber;
    std::vector<char> data;
    DataPacket(uint16_t blockNumber, const std::vector<char>& data);
    std::vector<char> serialize() const override;
    static DataPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    uint16_t getOpcode() const override { return 3; } // DATA opcode
    void handleClient(ClientSession& session) const override;
    void handleServer(ServerSession& session) const override;
};

class ACKPacket : public Packet {
public:
    uint16_t blockNumber;
    ACKPacket(uint16_t blockNumber);
    std::vector<char> serialize() const override;
    static ACKPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    uint16_t getOpcode() const override { return 4; } // ACK opcode
    void handleClient(ClientSession& session) const override;
    void handleServer(ServerSession& session) const override;
};

class ErrorPacket : public Packet {
public:
    uint16_t errorCode;
    std::string errorMessage;
    ErrorPacket(uint16_t errorCode, const std::string& errorMessage);
    std::vector<char> serialize() const override;
    static ErrorPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    uint16_t getOpcode() const override { return 5; } // ERROR opcode
    void handleClient(ClientSession& session) const override;
    void handleServer(ServerSession& session) const override;
};

class OACKPacket : public Packet {
public:
    std::map<std::string, int> options;
    OACKPacket(std::map<std::string, int> options);
    std::vector<char> serialize() const override;
    static OACKPacket parse(sockaddr_in addr, const char* buffer, size_t size);
    uint16_t getOpcode() const override { return 6; } // OACK opcode
    void handleClient(ClientSession& session) const override;
    void handleServer(ServerSession& session) const override;
};

#endif // PACKETS_HPP