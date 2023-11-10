#ifndef PACKETS_HPP
#define PACKETS_HPP

#include <vector>
#include <string>

class TFTPPacket {
public:
    virtual ~TFTPPacket() = default;
    virtual std::vector<char> serialize() const = 0;
    virtual void handlePacket() const = 0;

    static std::unique_ptr<TFTPPacket> parse(const char* buffer, size_t bufferSize);
};

class TFTPRequestPacket : public TFTPPacket {
public:
    std::string filename;
    std::string mode;
    virtual uint16_t getOpcode() const = 0;
    TFTPRequestPacket(const std::string& filename, const std::string& mode);
    std::vector<char> serialize() const override;
};

class TFTPReadRequestPacket : public TFTPRequestPacket {
public:
    using TFTPRequestPacket::TFTPRequestPacket;
    uint16_t getOpcode() const override { return 1; } // RRQ opcode
};

class TFTPWriteRequestPacket : public TFTPRequestPacket {
public:
    using TFTPRequestPacket::TFTPRequestPacket;
    uint16_t getOpcode() const override { return 2; } // WRQ opcode
};

class TFTPDataPacket : public TFTPPacket {
public:
    uint16_t blockNumber;
    std::vector<char> data;
    TFTPDataPacket(uint16_t blockNumber, const std::vector<char>& data);
    std::vector<char> serialize() const override;
};

class TFTPACKPacket : public TFTPPacket {
public:
    uint16_t blockNumber;
    TFTPACKPacket(uint16_t blockNumber);
    std::vector<char> serialize() const override;
    void handlePacket() const override;

    static TFTPACKPacket parse(const char* buffer, size_t size);
};

class TFTPErrorPacket : public TFTPPacket {
public:
    uint16_t errorCode;
    std::string errorMessage;
    TFTPErrorPacket(uint16_t errorCode, const std::string& errorMessage);
    void handlePacket() const override;
    std::vector<char> serialize() const override;
    static TFTPErrorPacket parse(const char* buffer, size_t size);
};

#endif // PACKETS_HPP