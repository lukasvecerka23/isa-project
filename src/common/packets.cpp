#include "common/packets.hpp"
#include "common/session.hpp"
#include "common/exceptions.hpp"
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/statvfs.h>


bool checkOptions(std::map<std::string, uint64_t> options){
    if (options.find("blksize") != options.end()){
        if (options["blksize"] < MIN_BLOCK_SIZE || options["blksize"] > MAX_BLOCK_SIZE){
            return false;
        }
    }
    if (options.find("timeout") != options.end()){
        if (options["timeout"] < MIN_TIMEOUT || options["timeout"] > MAX_TIMEOUT){
            return false;
        }
    }
    if (options.find("tsize") != options.end()){
        if (options["tsize"] < MIN_TSIZE || options["tsize"] > MAX_TSIZE){
            return false;
        }
    }
    return true;
}

std::pair<std::string, const char*> parseNetasciiString(const char* buffer, const char* start, const char* end) {
    std::string result;
    const char* current = start;

    while (current < end) {
        if (*current == '\r') {
            if ((current + 1) < end && *(current + 1) == '\0') {
                result.push_back('\r');
                current += 2;  // Skip the next character
            } else if ((current + 1) < end && *(current + 1) == '\n') {
                result.push_back('\n');
                current += 2;  // Skip the next character
            } else {
                result.push_back(*current);
                ++current;
            }
        } else if (*current == '\0') {
            ++current;  // Skip the null character
            break;
        } else {
            result.push_back(*current);
            ++current;
        }
    }

    return {result, current};
}

// PACKET
std::unique_ptr<Packet> Packet::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    if (bufferSize < 2) {
        throw ParsingError("Buffer too short to determine opcode");
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
        case 6: // OACK
            return std::make_unique<OACKPacket>(OACKPacket::parse(addr, buffer, bufferSize));
        default:
            throw ParsingError("Unknown or unhandled TFTP opcode");
    }
}

std::unique_ptr<Packet> createPacket(Packet& packet, int opcode) {
    switch (opcode) {
        case 1: return std::make_unique<ReadRequestPacket>(dynamic_cast<ReadRequestPacket&>(packet));
        case 2: return std::make_unique<WriteRequestPacket>(dynamic_cast<WriteRequestPacket&>(packet));
        case 3: return std::make_unique<DataPacket>(dynamic_cast<DataPacket&>(packet));
        case 4: return std::make_unique<ACKPacket>(dynamic_cast<ACKPacket&>(packet));
        case 5: return std::make_unique<ErrorPacket>(dynamic_cast<ErrorPacket&>(packet));
        case 6: return std::make_unique<OACKPacket>(dynamic_cast<OACKPacket&>(packet));
        default: return nullptr;
    }
}

void Packet::send(Session* session, int socket) {
    std::vector<char> message = this->serialize();
    ssize_t sentBytes = sendto(socket, message.data(), message.size(), 0, (struct sockaddr*)&addr, sizeof(addr));

    if (sentBytes < 0) {
        std::cout << "Failed to send data\n";
        return;
    }

    if (session != nullptr){
        session->lastPacket = createPacket(*this, this->getOpcode());
    }
}


// REQUEST PACKET
// Constructor for TFTPRequestPacket
RequestPacket::RequestPacket(const std::string& filename, DataMode mode, std::map<std::string, uint64_t> options, sockaddr_in addr)
    : filename(filename), mode(mode), options(options) {
        this->addr = addr;
    }

const std::set<std::string> RequestPacket::supportedOptions = {"blksize", "timeout", "tsize"};

// Serialize method for TFTPRequestPacket
std::vector<char> RequestPacket::serialize() const {
    std::vector<char> buffer;
    uint16_t opcode = getOpcode();
    buffer.push_back(0);
    buffer.push_back(opcode);
    buffer.insert(buffer.end(), filename.begin(), filename.end());
    buffer.push_back('\0');
    std::string modeStr = modeToString(mode);
    buffer.insert(buffer.end(), modeStr.begin(), modeStr.end());
    buffer.push_back('\0');

    // Add options
    std::string optMessage = "";
    for (const auto& option : options) {
        buffer.insert(buffer.end(), option.first.begin(), option.first.end());
        buffer.push_back('\0');
        std::string optionValueStr = std::to_string(option.second);
        buffer.insert(buffer.end(), optionValueStr.begin(), optionValueStr.end());
        buffer.push_back('\0');
        optMessage += option.first + "=" + optionValueStr + " ";
    }

    switch (opcode){
        case 1:
        {
            std::cout << "=> RRQ " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << filename << " " << modeToString(mode) << " " << optMessage << std::endl;
            break;
        }
        case 2:
        {
            std::cerr << "=> WRQ " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << filename << " " << modeToString(mode) << " " << optMessage <<std::endl;
            break;
        }
    }

    return buffer;
}

// WRITE REQUEST PACKET
// Constructor
WriteRequestPacket::WriteRequestPacket(const std::string& filename, DataMode mode, std::map<std::string, uint64_t> options, sockaddr_in addr)
    : RequestPacket(filename, mode, options, addr) {}

WriteRequestPacket WriteRequestPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing write request packet
    if (bufferSize < 4){
        throw ParsingError("Buffer too short for WRQ packet");
    }
    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 2) { // 2 is the opcode for WRQ
        throw ParsingError("Invalid opcode for DATA packet");
    }

    const char* current = buffer + 2;
    const char* filenameEnd = std::find(current, buffer + bufferSize, '\0');


    std::string filename(current, filenameEnd);
    if (filename.empty() || filenameEnd == buffer + bufferSize) {
        throw ParsingError("Invalid filename");
    }
    current = filenameEnd+1;

    const char* modeEnd = std::find(current, buffer + bufferSize, '\0');
    std::string modeStr(current, modeEnd);
    if (modeStr.empty() || modeEnd == buffer + bufferSize) {
        throw ParsingError("Invalid mode");
    }
    current = modeEnd + 1;
    DataMode mode = stringToMode(modeStr);


    std::map<std::string, uint64_t> options;
    std::string optMessage = "";
    while (current < buffer + bufferSize) {
        const char* optionEnd = std::find(current, buffer + bufferSize, '\0');
        std::string optionName(current, optionEnd);
        if (optionName.empty() || optionEnd == buffer + bufferSize) {
            throw OptionError("Invalid option name");
        }
        current = optionEnd + 1;

        const char* valueEnd = std::find(current, buffer + bufferSize, '\0');
        std::string optionValueStr(current, valueEnd);
        if (optionValueStr.empty() || valueEnd == buffer + bufferSize) {
            throw OptionError("Invalid option value");
        }
        current = valueEnd + 1;

        optMessage += optionName + "=" + optionValueStr + " ";

        if (supportedOptions.find(optionName) == supportedOptions.end()) {
            continue;
        }
        int optionValue;
        try{
            optionValue = std::stoi(optionValueStr);
        } catch (const std::exception& e){
            throw OptionError("Invalid option value");
        }
        options[optionName] = optionValue;
        
    }
    if (!checkOptions(options)){
        throw OptionError("Invalid option values");
    }

    std::cerr << "WRQ " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << filename << " " << modeStr << " " << optMessage << std::endl;
    return WriteRequestPacket(filename, mode, options, addr);
}

void WriteRequestPacket::handleClient(ClientSession* session) const {
    ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
    errorPacket.send(session, session->sessionSockfd);
    session->sessionState = SessionState::ERROR;
}

void WriteRequestPacket::handleServer(ServerSession* session) const {
    ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
    errorPacket.send(session, session->sessionSockfd);
    session->sessionState = SessionState::ERROR;
}

// READ REQUEST PACKET
// Constructor
ReadRequestPacket::ReadRequestPacket(const std::string& filename, DataMode mode, std::map<std::string, uint64_t> options, sockaddr_in addr)
    : RequestPacket(filename, mode, options, addr) {}

ReadRequestPacket ReadRequestPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing read request packet
    if (bufferSize < 4){
        throw ParsingError("Buffer too short for RRQ packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 1) { // 1 is the opcode for RRQ
        throw ParsingError("Invalid opcode for DATA packet");
    }

    const char* current = buffer + 2;
    const char* filenameEnd = std::find(current, buffer + bufferSize, '\0');


    std::string filename(current, filenameEnd);
    if (filename.empty() || filenameEnd == buffer + bufferSize) {
        throw ParsingError("Invalid filename");
    }
    current = filenameEnd+1;

    const char* modeEnd = std::find(current, buffer + bufferSize, '\0');
    std::string modeStr(current, modeEnd);
    if (modeStr.empty() || modeEnd == buffer + bufferSize) {
        throw ParsingError("Invalid mode");
    }
    current = modeEnd + 1;
    DataMode mode = stringToMode(modeStr);

    std::map<std::string, uint64_t> options;
    std::string optMessage = "";
    while (current < buffer + bufferSize) {
        const char* optionEnd = std::find(current, buffer + bufferSize, '\0');
        std::string optionName(current, optionEnd);
        if (optionName.empty() || optionEnd == buffer + bufferSize) {
            throw OptionError("Invalid option name");
        }
        current = optionEnd + 1;

        const char* valueEnd = std::find(current, buffer + bufferSize, '\0');
        std::string optionValueStr(current, valueEnd);
        if (optionValueStr.empty() || valueEnd == buffer + bufferSize) {
            throw OptionError("Invalid option value");
        }
        current = valueEnd + 1;

        optMessage += optionName + "=" + optionValueStr + " ";

        if (supportedOptions.find(optionName) == supportedOptions.end()) {
            continue;
        }
        int optionValue;
        try{
            optionValue = std::stoi(optionValueStr);
        } catch (const std::exception& e){
            throw OptionError("Invalid option value");
        }
        options[optionName] = optionValue;
    }

    if (!checkOptions(options)){
        throw OptionError("Invalid option values");
    }
    std::cerr << "RRQ " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << filename << " " << modeStr << optMessage << std::endl;
    return ReadRequestPacket(filename, mode, options, addr);
}

void ReadRequestPacket::handleClient(ClientSession* session) const {
    ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
    errorPacket.send(session, session->sessionSockfd);
    session->sessionState = SessionState::ERROR;
}

void ReadRequestPacket::handleServer(ServerSession* session) const {
    ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
    errorPacket.send(session, session->sessionSockfd);
    session->sessionState = SessionState::ERROR;
}


// DATA PACKET
// Constructor
DataPacket::DataPacket(uint16_t blockNumber, const std::vector<char>& data, sockaddr_in addr)
    : blockNumber(blockNumber), data(data) {
        this->addr = addr;
    }

DataPacket DataPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing data packet
    if (bufferSize < 4){
        throw ParsingError("Buffer too short for DATA packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 3) { // 3 is the opcode for DATA
        throw ParsingError("Invalid opcode for DATA packet");
    }

    uint16_t blockNumber = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    std::vector<char> data(buffer + 4, buffer + bufferSize);
    std::cerr << "DATA " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << blockNumber << std::endl;
    return DataPacket(blockNumber, data, addr);
}

// Serialize method for TFTPDataPacket
std::vector<char> DataPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(3); // Opcode for DATA
    buffer.push_back((blockNumber >> 8) & 0xFF);
    buffer.push_back(blockNumber & 0xFF);
    buffer.insert(buffer.end(), data.begin(), data.end());
    std::cout << "=> DATA " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << blockNumber << std::endl;
    return buffer;
}

void DataPacket::handleClient(ClientSession* session) const {
    switch(session->sessionState){
        case SessionState::INITIAL:
        {
            if (session->blockNumber == this->blockNumber){
                try {
                    session->writeDataBlock(data);
                } catch (const std::exception& e) {
                    ErrorPacket errorPacket(3, "Disk full or allocation exceeded", session->dst_addr);
                    errorPacket.send(session, session->sessionSockfd);
                    session->sessionState = SessionState::ERROR;
                    break;
                }
                
                if (data.size() < session->blockSize){
                    session->sessionState = SessionState::RRQ_END;
                    break;
                }
                ACKPacket ackPacket(session->blockNumber, session->dst_addr);
                ackPacket.send(session, session->sessionSockfd);
                session->blockNumber++;
                session->sessionState = SessionState::WAITING_DATA;
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        case SessionState::WAITING_DATA:
        {
            if (session->blockNumber == this->blockNumber){
                try {
                    session->writeDataBlock(data);
                } catch (const std::exception& e) {
                    ErrorPacket errorPacket(3, "Disk full or allocation exceeded", session->dst_addr);
                    errorPacket.send(session, session->sessionSockfd);
                    session->sessionState = SessionState::ERROR;
                    break;
                }
                
                if (data.size() < session->blockSize){
                    session->writeStream.close();
                    session->sessionState = SessionState::RRQ_END;
                }
                ACKPacket ackPacket(session->blockNumber, session->dst_addr);
                ackPacket.send(session, session->sessionSockfd);
                session->blockNumber++;
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        case SessionState::WAITING_OACK:
        {
            if (session->blockNumber == this->blockNumber){
                try{
                    session->writeDataBlock(data);
                } catch (const std::exception& e) {
                    ErrorPacket errorPacket(3, "Disk full or allocation exceeded", session->dst_addr);
                    errorPacket.send(session, session->sessionSockfd);
                    session->sessionState = SessionState::ERROR;
                    break;
                }
                
                if (data.size() < session->blockSize){
                    session->sessionState = SessionState::RRQ_END;
                    break;
                }
                ACKPacket ackPacket(session->blockNumber, session->dst_addr);
                ackPacket.send(session, session->sessionSockfd);
                session->blockNumber++;
                session->sessionState = SessionState::WAITING_DATA;
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        default:
        {
            ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
            errorPacket.send(session, session->sessionSockfd);
            session->sessionState = SessionState::ERROR;
        }
    
    }
}

void DataPacket::handleServer(ServerSession* session) const {
    switch(session->sessionState){
        case SessionState::WAITING_DATA:
        {
            if (session->blockNumber == this->blockNumber){
                try{
                    session->writeDataBlock(data);
                } catch (const std::exception& e) {
                    ErrorPacket errorPacket(3, "Disk full or allocation exceeded", session->dst_addr);
                    errorPacket.send(session, session->sessionSockfd);
                    session->sessionState = SessionState::ERROR;
                    break;
                }
                
                if (data.size() < session->blockSize){
                    session->writeStream.close();
                    session->sessionState = SessionState::WRQ_END;
                }
                ACKPacket ackPacket(session->blockNumber, session->dst_addr);
                ackPacket.send(session, session->sessionSockfd);
                session->blockNumber++;
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        default:
        {
            ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
            errorPacket.send(session, session->sessionSockfd);
            session->sessionState = SessionState::ERROR;
        }
    }
}


// ACK PACKET
// Constructor
ACKPacket::ACKPacket(uint16_t blockNumber, sockaddr_in addr) : blockNumber(blockNumber) {
    this->addr = addr;
}

// Serialize method
std::vector<char> ACKPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(4); // Opcode for ACK
    buffer.push_back((blockNumber >> 8) & 0xFF);
    buffer.push_back(blockNumber & 0xFF);
    std::cout << "=> ACK " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << blockNumber << std::endl;
    return buffer;
}

// Static parse method implementation
ACKPacket ACKPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    if (bufferSize < 4) {
        throw ParsingError("Buffer too short for ACK packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 4) { // 4 is the opcode for ACK
        throw ParsingError("Invalid opcode for ACK packet");
    }

    uint16_t blockNumber = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    std::cerr << "ACK " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << blockNumber << std::endl;
    return ACKPacket(blockNumber, addr);
}

void ACKPacket::handleClient(ClientSession* session) const {
    switch(session->sessionState){
        case SessionState::INITIAL:
        {
            session->blockNumber = 1;
            std::vector<char> data = session->readDataBlock();
            DataPacket dataPacket(session->blockNumber, data, session->dst_addr);
            dataPacket.send(session, session->sessionSockfd);
            if (data.size() < session->blockSize){
                session->sessionState = SessionState::WAITING_LAST_ACK;
                break;
            }
            session->sessionState = SessionState::WAITING_ACK;
            break;
        }
        case SessionState::WAITING_ACK:
        {
            if (session->blockNumber == this->blockNumber){
                session->blockNumber++;
                std::vector<char> data = session->readDataBlock();
                DataPacket dataPacket(session->blockNumber, data, session->dst_addr);
                dataPacket.send(session, session->sessionSockfd);
                if (data.size() < session->blockSize){
                    session->sessionState = SessionState::WAITING_LAST_ACK;
                    break;
                }
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        case SessionState::WAITING_LAST_ACK:
        {
            if (session->blockNumber == this->blockNumber){
                std::cout << "File transfer complete" << std::endl;
                session->sessionState = SessionState::WRQ_END;
            } else {
                ErrorPacket errorPacket(5, "Invalid block number", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        case SessionState::WAITING_OACK:
        {
            if (session->blockNumber == this->blockNumber){
                session->blockNumber++;
                std::vector<char> data = session->readDataBlock();
                DataPacket dataPacket(session->blockNumber, data, session->dst_addr);
                dataPacket.send(session, session->sessionSockfd);
                if (data.size() < session->blockSize){
                    session->sessionState = SessionState::WAITING_LAST_ACK;
                    break;
                }
                session->sessionState = SessionState::WAITING_ACK;
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        default:
        {
            ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
            errorPacket.send(session, session->sessionSockfd);
            session->sessionState = SessionState::ERROR;
        }
    }
}

void ACKPacket::handleServer(ServerSession* session) const {
    switch(session->sessionState){
        case SessionState::WAITING_ACK:
        {
            if (session->blockNumber == this->blockNumber){
                session->blockNumber++;
                std::vector<char> data = session->readDataBlock();
                DataPacket dataPacket(session->blockNumber, data, session->dst_addr);
                dataPacket.send(session, session->sessionSockfd);
                if (data.size() < session->blockSize){
                    session->sessionState = SessionState::WAITING_LAST_ACK;
                    break;
                }
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        case SessionState::WAITING_LAST_ACK:
        {
            if (session->blockNumber == this->blockNumber){
                std::cout << "File transfer complete" << std::endl;
                session->readStream.close();
                session->sessionState = SessionState::RRQ_END;
            } else {
                ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
            break;
        }
        default:
        {
            ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
            errorPacket.send(session, session->sessionSockfd);
            session->sessionState = SessionState::ERROR;
        }
    }
}


// ERROR PACKET
// Constructor for ErrorPacket
ErrorPacket::ErrorPacket(uint16_t errorCode, const std::string& errorMessage, sockaddr_in addr)
    : errorCode(errorCode), errorMessage(errorMessage) {
        this->addr = addr;
    }

// Static parse method implementation
ErrorPacket ErrorPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing error packet
    if (bufferSize < 5){
        throw ParsingError("Buffer too short for ERROR packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 5) { // 4 is the opcode for ACK
        throw ParsingError("Invalid opcode for ACK packet");
    }

    uint16_t errorCode = (static_cast<uint8_t>(buffer[2]) << 8) | static_cast<uint8_t>(buffer[3]);
    const char* errorMessageEnd = std::find(buffer + 4, buffer + bufferSize, '\0');
    std::string errorMessage(buffer + 4, errorMessageEnd);
    if (errorMessageEnd == buffer + bufferSize) {
        throw ParsingError("Invalid error message");
    }
    std::cerr << "ERROR " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << errorCode << " " << errorMessage << std::endl;
    return ErrorPacket(errorCode, errorMessage, addr);
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

    std::cout << "=> ERROR " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << errorCode << " " << errorMessage << std::endl;
    return buffer;
}

void ErrorPacket::handleClient(ClientSession* session) const {
    session->sessionState = SessionState::ERROR;
}

void ErrorPacket::handleServer(ServerSession* session) const {
    session->sessionState = SessionState::ERROR;
}

OACKPacket::OACKPacket(std::map<std::string, uint64_t> options, sockaddr_in addr)
    : options(options) {
        this->addr = addr;
    }



std::vector<char> OACKPacket::serialize() const {
    std::vector<char> buffer;
    buffer.push_back(0);
    buffer.push_back(6); // Opcode for OACK
    std::string optMessage = "";
    for (const auto& option : options) {
        buffer.insert(buffer.end(), option.first.begin(), option.first.end());
        buffer.push_back('\0');
        std::string optionValueStr = std::to_string(option.second);
        buffer.insert(buffer.end(), optionValueStr.begin(), optionValueStr.end());
        buffer.push_back('\0');
        optMessage += option.first + "=" + std::to_string(option.second) + " ";
    }
    std::cout << "=> OACK " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << optMessage << std::endl;
    return buffer;
}

OACKPacket OACKPacket::parse(sockaddr_in addr, const char* buffer, size_t bufferSize) {
    // parsing OACK packet
    if (bufferSize < 4){
        throw ParsingError("Buffer too short for OACK packet");
    }

    uint16_t opcode = (static_cast<uint8_t>(buffer[0]) << 8) | static_cast<uint8_t>(buffer[1]);
    if (opcode != 6) { // 4 is the opcode for ACK
        throw ParsingError("Invalid opcode for ACK packet");
    }

    const char* current = buffer + 2;


    std::map<std::string, uint64_t> options;
    std::string optMessage = "";
    while (current < buffer + bufferSize) {
        const char* optionEnd = std::find(current, buffer + bufferSize, '\0');
        std::string optionName(current, optionEnd);
        if (optionName.empty() || optionEnd == buffer + bufferSize) {
            throw OptionError("Invalid option name");
        }
        current = optionEnd + 1;

        const char* valueEnd = std::find(current, buffer + bufferSize, '\0');
        std::string optionValueStr(current, valueEnd);
        if (optionValueStr.empty() || valueEnd == buffer + bufferSize) {
            throw OptionError("Invalid option value");
        }
        current = valueEnd + 1;

        optMessage += optionName + "=" + optionValueStr + " ";

        if (RequestPacket::supportedOptions.find(optionName) == RequestPacket::supportedOptions.end()) {
            continue;
        }
        int optionValue;
        try{
            optionValue = std::stoi(optionValueStr);
        } catch (const std::exception& e){
            throw OptionError("Invalid option value");
        }
        options[optionName] = optionValue;
    }

    if (!checkOptions(options)){
        throw OptionError("Invalid option values");
    }

    std::cerr << "OACK " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << " " << optMessage << std::endl;
    return OACKPacket(options, addr);
}

void OACKPacket::handleClient(ClientSession* session) const {
    if (session->sessionState == SessionState::WAITING_OACK){
        for (const auto& option : options) {
            if (session->options.find(option.first) == session->options.end()) {
                ErrorPacket errorPacket(8, "Unknown transfer option", session->dst_addr);
                errorPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::ERROR;
            }
        }
        session->setOptions(options);
        switch(session->sessionType){
            case SessionType::READ:
            {
                // if tsize option is set check if there is enough space on disk
                if (options.find("tsize") != options.end()){
                    struct statvfs stat;
                    if (statvfs("/", &stat) != 0) {
                        // Error occurred getting filesystem stats
                        ErrorPacket errorPacket(3, "Disk full or allocation exceeded", session->dst_addr);
                        errorPacket.send(session, session->sessionSockfd);
                        return;
                    }

                    uint64_t freeSpace = stat.f_bsize * stat.f_bfree;
                    if (freeSpace < options.at("tsize")) {
                        // Not enough disk space
                        ErrorPacket errorPacket(3, "Disk full or allocation exceeded", session->dst_addr);
                        errorPacket.send(session, session->sessionSockfd);
                        return;
                    }
                }
                ACKPacket ackPacket(0, session->dst_addr);
                ackPacket.send(session, session->sessionSockfd);
                session->sessionState = SessionState::WAITING_DATA;
                break;
            }
            case SessionType::WRITE:
            {
                session->blockNumber++;
                std::vector<char> data = session->readDataBlock();
                DataPacket dataPacket(session->blockNumber, data, session->dst_addr);
                dataPacket.send(session, session->sessionSockfd);
                if (data.size() < session->blockSize){
                    session->sessionState = SessionState::WAITING_LAST_ACK;
                    break;
                }
                session->sessionState = SessionState::WAITING_ACK;
                break;
            }
        }
    } else {
        ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
        errorPacket.send(session, session->sessionSockfd);
        session->sessionState = SessionState::ERROR;
    }
}

void OACKPacket::handleServer(ServerSession* session) const {
    ErrorPacket errorPacket(4, "Illegal TFTP operation", session->dst_addr);
    errorPacket.send(session, session->sessionSockfd);
    session->sessionState = SessionState::ERROR;
}