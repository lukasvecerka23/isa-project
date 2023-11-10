#include "common/packets.hpp"
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <string>

std::unique_ptr<TFTPPacket> TFTPPacket::parse(const char* buffer, size_t bufferSize) {
    if (bufferSize < 2) {
        throw std::runtime_error("Buffer too short to determine opcode");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);

    switch (opcode) {
        case 4: // ACK
            return std::make_unique<TFTPACKPacket>(TFTPACKPacket::parse(buffer, bufferSize));
        case 5: // ERROR
            return std::make_unique<TFTPErrorPacket>(TFTPErrorPacket::parse(buffer, bufferSize));
        // Handle other opcodes (RRQ, WRQ, DATA, ERROR) similarly
        default:
            throw std::runtime_error("Unknown or unhandled TFTP opcode");
    }
}

// Constructor for TFTPRequestPacket
TFTPRequestPacket::TFTPRequestPacket(const std::string& filename, const std::string& mode)
    : filename(filename), mode(mode) {}

// Serialize method for TFTPRequestPacket
std::vector<char> TFTPRequestPacket::serialize() const {
    std::vector<char> buffer;
    uint16_t opcode = getOpcode();
    buffer.push_back((opcode >> 8) & 0xFF);
    buffer.push_back(opcode & 0xFF);
    buffer.insert(buffer.end(), filename.begin(), filename.end());
    buffer.push_back('\0');
    buffer.insert(buffer.end(), mode.begin(), mode.end());
    buffer.push_back('\0');
    return buffer;
}

// Constructor for TFTPDataPacket
TFTPDataPacket::TFTPDataPacket(uint16_t blockNumber, const std::vector<char>& data)
    : blockNumber(blockNumber), data(data) {}

// Serialize method for TFTPDataPacket
std::vector<char> TFTPDataPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(3); // Opcode for DATA
    buffer.push_back((blockNumber >> 8) & 0xFF);
    buffer.push_back(blockNumber & 0xFF);
    buffer.insert(buffer.end(), data.begin(), data.end());
    return buffer;
}

// Constructor for TFTPACKPacket
TFTPACKPacket::TFTPACKPacket(uint16_t blockNumber) : blockNumber(blockNumber) {}

// Serialize method for TFTPACKPacket
std::vector<char> TFTPACKPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(4); // Opcode for ACK
    buffer.push_back((blockNumber >> 8) & 0xFF);
    buffer.push_back(blockNumber & 0xFF);
    return buffer;
}

// Static parse method implementation
TFTPACKPacket TFTPACKPacket::parse(const char* buffer, size_t bufferSize) {
    if (bufferSize < 4) {
        throw std::runtime_error("Buffer too short for ACK packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 4) { // 4 is the opcode for ACK
        throw std::runtime_error("Invalid opcode for ACK packet");
    }

    uint16_t blockNumber = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    return TFTPACKPacket(blockNumber);
}

void TFTPACKPacket::handlePacket() const {
    std::cout << "Received ACK packet with block number: " << blockNumber << std::endl;
}

// Constructor for TFTPErrorPacket
TFTPErrorPacket::TFTPErrorPacket(uint16_t errorCode, const std::string& errorMessage)
    : errorCode(errorCode), errorMessage(errorMessage) {}

// Static parse method implementation
TFTPErrorPacket TFTPErrorPacket::parse(const char* buffer, size_t bufferSize) {
    // parsing error packet
    if (bufferSize < 5){
        throw std::runtime_error("Buffer too short for ERROR packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 5) { // 4 is the opcode for ACK
        throw std::runtime_error("Invalid opcode for ACK packet");
    }

    uint16_t errorCode = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    std::string errorMessage = std::string(buffer + 4, buffer + bufferSize - 1);
    return TFTPErrorPacket(errorCode, errorMessage);
}

void TFTPErrorPacket::handlePacket() const {
    std::cerr << "Received ERROR packet with error code: " << errorCode 
              << " and error message: " << errorMessage << std::endl;
}

// Serialize method for TFTPErrorPacket
std::vector<char> TFTPErrorPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(5); // Opcode for ERROR
    buffer.push_back((errorCode >> 8) & 0xFF);
    buffer.push_back(errorCode & 0xFF);
    buffer.insert(buffer.end(), errorMessage.begin(), errorMessage.end());
    buffer.push_back('\0');
    return buffer;
}