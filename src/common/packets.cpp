#include "common/packets.hpp"
#include "common/session.hpp"
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


// PACKET
std::unique_ptr<Packet> Packet::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    if (bufferSize < 2) {
        throw std::runtime_error("Buffer too short to determine opcode");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);

    switch (opcode) {
        case 1: // RRQ
            return std::make_unique<ReadRequestPacket>(ReadRequestPacket::parse(addr, buffer, bufferSize));
        case 2: // WRQ
            return std::make_unique<WriteRequestPacket>(WriteRequestPacket::parse(addr, buffer, bufferSize));
        case 3: // DATA
            return std::make_unique<DataPacket>(DataPacket::parse(addr, buffer, bufferSize));
        case 4: // ACK
            return std::make_unique<ACKPacket>(ACKPacket::parse(addr, buffer, bufferSize));
        case 5: // ERROR
            return std::make_unique<ErrorPacket>(ErrorPacket::parse(addr, buffer, bufferSize));
        default:
            throw std::runtime_error("Unknown or unhandled TFTP opcode");
    }
}

void Packet::send(int socket, sockaddr_in dst_addr) const {
    std::vector<char> message = serialize();
    std::cout << "data: " << message.data() << "size: " << message.size() <<  std::endl;
    ssize_t sentBytes = sendto(socket, message.data(), message.size(), 0, (struct sockaddr*)&dst_addr, sizeof(dst_addr));

    if (sentBytes < 0) {
        std::cout << "Failed to send data\n";
        return;
    }
}


// REQUEST PACKET
// Constructor for TFTPRequestPacket
RequestPacket::RequestPacket(const std::string& filename, const std::string& mode)
    : filename(filename), mode(mode) {}

// Serialize method for TFTPRequestPacket
std::vector<char> RequestPacket::serialize() const {
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

// WRITE REQUEST PACKET
// Constructor
WriteRequestPacket::WriteRequestPacket(const std::string& filename, const std::string& mode)
    : RequestPacket(filename, mode) {}

WriteRequestPacket WriteRequestPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing write request packet
    std::cout << bufferSize << std::endl;
    if (bufferSize < 4){
        throw std::runtime_error("Buffer too short for WRQ packet");
    }

    const char* filenameEnd = std::find(buffer + 2, buffer + bufferSize, '\0');

    std::string filename = std::string(buffer + 2, filenameEnd);
    std::string mode = std::string(buffer + 2 + filename.size() + 1, buffer + bufferSize - 1);
    std::cerr << "WRQ " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << filename << " " << mode << std::endl;
    return WriteRequestPacket(filename, mode);
}

void WriteRequestPacket::handleClient(ClientSession& session) const {
    std::cout << "Write request packet" << std::endl;
}

void WriteRequestPacket::handleServer(ServerSession& session) const {
    std::cout << "Write request packet" << std::endl;
}

// READ REQUEST PACKET
// Constructor
ReadRequestPacket::ReadRequestPacket(const std::string& filename, const std::string& mode)
    : RequestPacket(filename, mode) {}

ReadRequestPacket ReadRequestPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing read request packet
    if (bufferSize < 4){
        throw std::runtime_error("Buffer too short for RRQ packet");
    }

    const char* filenameEnd = std::find(buffer + 2, buffer + bufferSize, '\0');

    std::string filename = std::string(buffer + 2, filenameEnd);
    std::string mode = std::string(buffer + 2 + filename.size() + 1, buffer + bufferSize - 1);
    std::cerr << "RRQ " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << filename << " " << mode << std::endl;
    return ReadRequestPacket(filename, mode);
}

void ReadRequestPacket::handleClient(ClientSession& session) const {
    std::cout << "Read request packet" << std::endl;
}

void ReadRequestPacket::handleServer(ServerSession& session) const {
    std::cout << "Read request packet" << std::endl;
}


// DATA PACKET
// Constructor
DataPacket::DataPacket(uint16_t blockNumber, const std::vector<char>& data)
    : blockNumber(blockNumber), data(data) {}

DataPacket DataPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing data packet
    if (bufferSize < 4){
        throw std::runtime_error("Buffer too short for DATA packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 3) { // 3 is the opcode for DATA
        throw std::runtime_error("Invalid opcode for DATA packet");
    }

    uint16_t blockNumber = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    std::vector<char> data(buffer + 4, buffer + bufferSize);
    std::cerr << "DATA " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << blockNumber << std::endl;
    return DataPacket(blockNumber, data);
}

// Serialize method for TFTPDataPacket
std::vector<char> DataPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(3); // Opcode for DATA
    buffer.push_back((blockNumber >> 8) & 0xFF);
    buffer.push_back(blockNumber & 0xFF);
    buffer.insert(buffer.end(), data.begin(), data.end());
    return buffer;
}

void DataPacket::handleClient(ClientSession& session) const {
    std::cout << "Data packet" << std::endl;
}

void DataPacket::handleServer(ServerSession& session) const {
    switch(session.sessionState){
        case SessionState::WAITING_DATA:
        {
            if (session.blockNumber == this->blockNumber){
                session.fileStream.write(data.data(), data.size());
                if (!session.fileStream.good()) {
                    throw std::runtime_error("Failed to write data to file");
                    break;
                }
                std::cout << "Wrote " << data.data() << " to file" << std::endl;

                if (data.size() < session.blockSize){
                    session.sessionState = SessionState::END;
                    break;
                }
                ACKPacket ackPacket(session.blockNumber);
                ackPacket.send(session.sessionSockfd, session.dst_addr);
                session.blockNumber++;
            }
            break;
        }
        default:
        {
            std::cout << "Bad state" << std::endl;
            break;
        }
    }
}


// ACK PACKET
// Constructor
ACKPacket::ACKPacket(uint16_t blockNumber) : blockNumber(blockNumber) {}

// Serialize method
std::vector<char> ACKPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(4); // Opcode for ACK
    buffer.push_back((blockNumber >> 8) & 0xFF);
    buffer.push_back(blockNumber & 0xFF);
    return buffer;
}

// Static parse method implementation
ACKPacket ACKPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    if (bufferSize < 4) {
        throw std::runtime_error("Buffer too short for ACK packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 4) { // 4 is the opcode for ACK
        throw std::runtime_error("Invalid opcode for ACK packet");
    }

    uint16_t blockNumber = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    std::cerr << "ACK " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << blockNumber << std::endl;
    return ACKPacket(blockNumber);
}

void ACKPacket::handleClient(ClientSession& session) const {
    switch(session.sessionState){
        case SessionState::INITIAL:
        {
            session.srcTID = ntohs(session.dst_addr.sin_port);
            std::cout << "srcTID: " << session.srcTID << std::endl;
            session.blockNumber = 1;
            std::vector<char> data(session.blockSize);
            int bytesRead = session.readDataBlock(data);
            DataPacket dataPacket(session.blockNumber, data);
            dataPacket.send(session.sessionSockfd, session.dst_addr);
            if (bytesRead < session.blockSize){
                session.sessionState = SessionState::END;
                break;
            }
            session.sessionState = SessionState::WAITING_ACK;
            break;
        }
        case SessionState::WAITING_ACK:
        {
            if (session.blockNumber == this->blockNumber){
                session.blockNumber++;
                std::vector<char> data(session.blockSize);
                int bytesRead = session.readDataBlock(data);
                DataPacket dataPacket(session.blockNumber, data);
                dataPacket.send(session.sessionSockfd, session.dst_addr);
                if (bytesRead < session.blockSize){
                    session.sessionState = SessionState::END;
                    break;
                }
            }
            break;
        }
        case SessionState::END:
        {
            break;
        }
        default:
        {
            std::cout << "Bad state" << std::endl;
            break;
        }
    }
}

void ACKPacket::handleServer(ServerSession& session) const {
    std::cout << "Ack packet" << std::endl;
}


// ERROR PACKET
// Constructor for ErrorPacket
ErrorPacket::ErrorPacket(uint16_t errorCode, const std::string& errorMessage)
    : errorCode(errorCode), errorMessage(errorMessage) {}

// Static parse method implementation
ErrorPacket ErrorPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
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
    std::cerr << "ERROR " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << errorCode << " " << errorMessage << std::endl;
    return ErrorPacket(errorCode, errorMessage);
}

// Serialize method for TFTPErrorPacket
std::vector<char> ErrorPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(5); // Opcode for ERROR
    buffer.push_back((errorCode >> 8) & 0xFF);
    buffer.push_back(errorCode & 0xFF);
    buffer.insert(buffer.end(), errorMessage.begin(), errorMessage.end());
    buffer.push_back('\0');
    return buffer;
}

void ErrorPacket::handleClient(ClientSession& session) const {
    std::cout << "Error packet" << std::endl;
}

void ErrorPacket::handleServer(ServerSession& session) const {
    std::cout << "Error packet" << std::endl;
}