#ifndef PACKETS_HPP
#define PACKETS_HPP

#include <vector>
#include <string>

class Packet {
public:
    virtual ~Packet() = default;
    virtual std::vector<char> serialize() const = 0;
    virtual void handlePacket() const = 0;

    static std::unique_ptr<Packet> parse(const char* buffer, size_t bufferSize);
};

class RequestPacket : public Packet {
public:
    std::string filename;
    std::string mode;
    virtual uint16_t getOpcode() const = 0;
    RequestPacket(const std::string& filename, const std::string& mode);
    std::vector<char> serialize() const override;
};

class ReadRequestPacket : public RequestPacket {
public:
    using RequestPacket::RequestPacket;
    uint16_t getOpcode() const override { return 1; } // RRQ opcode
};

class WriteRequestPacket : public RequestPacket {
public:
    using RequestPacket::RequestPacket;
    uint16_t getOpcode() const override { return 2; } // WRQ opcode
};

class DataPacket : public Packet {
public:
    uint16_t blockNumber;
    std::vector<char> data;
    DataPacket(uint16_t blockNumber, const std::vector<char>& data);
    std::vector<char> serialize() const override;
};

class ACKPacket : public Packet {
public:
    uint16_t blockNumber;
    ACKPacket(uint16_t blockNumber);
    std::vector<char> serialize() const override;
    void handlePacket() const override;

    static ACKPacket parse(const char* buffer, size_t size);
};

class ErrorPacket : public Packet {
public:
    uint16_t errorCode;
    std::string errorMessage;
    ErrorPacket(uint16_t errorCode, const std::string& errorMessage);
    void handlePacket() const override;
    std::vector<char> serialize() const override;
    static ErrorPacket parse(const char* buffer, size_t size);
};

#endif // PACKETS_HPP