#include "common/session.hpp"
#include "common/packets.hpp"
#include "common/exceptions.hpp"
#include <sys/statvfs.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>

// Define stopFlag
std::shared_ptr<std::atomic<bool>> stopFlag = std::make_shared<std::atomic<bool>>(false);

std::string modeToString(DataMode value) {
    switch (value) {
        case DataMode::NETASCII: return "netascii";
        case DataMode::OCTET: return "octet";
        default: return "unknown";
    }
}

DataMode stringToMode(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "netascii") {
        return DataMode::NETASCII;
    } else if (value == "octet") {
        return DataMode::OCTET;
    } else {
        throw ParsingError("Invalid mode");
    }
}

Session::Session(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType)
{
    this->dst_addr = dst_addr;
    this->srcTID = ntohs(dst_addr.sin_port);
    this->blockSize = 512;
    this->blockNumber = 0;
    this->src_filename = src_filename;
    this->dst_filename = dst_filename;
    this->dataMode = dataMode;
    this->sessionType = sessionType;
    this->sessionSockfd = socket;
    this->sessionState = SessionState::INITIAL;
}

ClientSession::ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, uint64_t> options)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {
        this->options = options;
        this->TIDisSet = false;
    }

void ClientSession::handleSession() {
    if (sessionType == SessionType::READ){
        std::cout << "Opening file on client: " << dst_filename << std::endl;
        this->writeStream.open(dst_filename, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!writeStream.is_open()) {
            std::cout << "Failed to open file: " << dst_filename << std::endl;
            return;
        }
        blockNumber = 1;
    }
    if (!options.empty()){
        sessionState = SessionState::WAITING_OACK;
    }

    char buffer[BUFFER_SIZE];
    socklen_t dst_len = sizeof(dst_addr);
    while(true){
        if(stopFlag->load()){
            ErrorPacket errorPacket(0, "Server shutdown");
            errorPacket.send(sessionSockfd, dst_addr);
            this->exit();
            std::cout << "Server is shutting down exiting client session" << std::endl;
            return;
        }
        ssize_t n = recvfrom(sessionSockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&dst_addr, &dst_len);
        if (n < 0) {
            std::cerr << "Failed to receive data\n";
            return;
        }

        if (!TIDisSet){
            this->srcTID = ntohs(dst_addr.sin_port);
            TIDisSet = true;
        }

        int srcTID = ntohs(dst_addr.sin_port);
        if (srcTID != this->srcTID){
            ErrorPacket errorPacket(5, "Unknown transfer ID");
            errorPacket.send(sessionSockfd, dst_addr);
            continue;
        }

        std::unique_ptr<Packet> packet;
        try {
            packet = Packet::parse(dst_addr, buffer, n);
        } catch (const ParsingError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what());
            errorPacket.send(sessionSockfd, dst_addr);
            return;
        }
        catch (const OptionError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what());
            errorPacket.send(sessionSockfd, dst_addr);
            return;
        }
        catch (const std::exception& e) {
            ErrorPacket errorPacket(0, e.what());
            errorPacket.send(sessionSockfd, dst_addr);
            return;
        }

        packet->handleClient(*this);
        if (sessionState == SessionState::RRQ_END || sessionState == SessionState::WRQ_END || sessionState == SessionState::ERROR){
            this->exit();
            return;
        }
    }
}

std::vector<char> ClientSession::readDataBlock() {
    std::vector<char> data;
    switch (dataMode) {
        case DataMode::NETASCII:
            {
                char ch;
                while(std::cin.get(ch) && data.size() ){
                    if (std::cin.fail()){
                        throw std::runtime_error("Failed to read data from stdin");
                    }
                    if (ch == '\n'){
                        data.push_back('\r');
                        if (data.size() < blockSize){
                            data.push_back('\n');
                        }
                    } else if (ch == '\r'){
                        data.push_back('\r');
                        if (data.size() < blockSize){
                            data.push_back('\0');
                        }
                    } else {
                        data.push_back(ch);
                    }
                    break;
                }
            }
        case DataMode::OCTET:
            data.resize(blockSize);
            std::cin.read(data.data(), blockSize);
            ssize_t bytesRead = std::cin.gcount();
            
            if (bytesRead < 0) {
                throw std::runtime_error("Failed to read data from stdin");
            }

            data.resize(bytesRead);
            break;
    }

    return data;
}

void Session::writeDataBlock(std::vector<char> data) {
    switch (dataMode) {
        case DataMode::NETASCII:
            {
                auto [convertedData, _ ] = parseNetasciiString(data.data(), data.data(), data.data() + data.size());

                writeStream.write(convertedData.data(), convertedData.size());
                if (writeStream.fail()) {
                    throw std::runtime_error("Failed to write data to file");
                }
            }
            break;
        case DataMode::OCTET:
            {
                writeStream.write(data.data(), data.size());
                if (writeStream.fail()) {
                    throw std::runtime_error("Failed to write data to file");
                }
            }
            break;
    }
}

void ClientSession::setOptions(std::map<std::string, uint64_t> newOptions){
    options = newOptions;

    // Check if the options map contains the "blksize" option
    if (options.find("blksize") != options.end()) {
        // Set the blockSize member variable to its value
        std::cout << "Setting block size to " << options.at("blksize") << std::endl;
        this->blockSize = options.at("blksize");
    }

    // Check if the options map contains the "timeout" option
    if (options.find("timeout") != options.end()) {
        // Set the timeout to its value
        std::cout << "Setting timeout to " << options.at("timeout") << std::endl;
        this->timeout = options.at("timeout");
    }

    // Check if the options map contains the "tsize" option
    if (options.find("tsize") != options.end()) {
        // Set the tsize to its value
        std::cout << "Setting tsize to " << options.at("tsize") << std::endl;
        this->tsize = options.at("tsize");
    }
}

void ClientSession::exit(){
    std::cout << "Exiting client session" << std::endl;
    writeStream.close();
    close(sessionSockfd);
}

ServerSession::ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, uint64_t> options)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {
        this->options = options;

        // Check if the options map contains the "blksize" option
        if (options.find("blksize") != options.end()) {
            // Set the blockSize member variable to its value
            std::cout << "Setting block size to " << options.at("blksize") << std::endl;
            this->blockSize = options.at("blksize");
        }

        // Check if the options map contains the "timeout" option
        if (options.find("timeout") != options.end()) {
            // Set the timeout to its value
            std::cout << "Setting timeout to " << options.at("timeout") << std::endl;
            this->timeout = options.at("timeout");
        }

        // Check if the options map contains the "tsize" option
        if (options.find("tsize") != options.end()) {
            // Set the tsize to its value
            std::cout << "Setting tsize to " << options.at("tsize") << std::endl;
            this->tsize = options.at("tsize");
        }
    }

void ServerSession::handleSession() {
    char buffer[BUFFER_SIZE];
    if (sessionType == SessionType::WRITE){
        std::cout << "Opening file on server: " << dst_filename << std::endl;
        this->writeStream.open(dst_filename, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!writeStream.is_open()) {
            ErrorPacket errorPacket(2, "Access violation");
            errorPacket.send(sessionSockfd, dst_addr);
            return;
        }

        // If "tsize" is set, check if there is enough disk space
        if (options.find("tsize") != options.end()) {
            struct statvfs stat;
            if (statvfs("/", &stat) != 0) {
                // Error occurred getting filesystem stats
                ErrorPacket errorPacket(3, "Disk full or allocation exceeded");
                errorPacket.send(sessionSockfd, dst_addr);
                return;
            }

            uint64_t freeSpace = stat.f_bsize * stat.f_bfree;
            if (freeSpace < options.at("tsize")) {
                // Not enough disk space
                ErrorPacket errorPacket(3, "Disk full or allocation exceeded");
                errorPacket.send(sessionSockfd, dst_addr);
                return;
            }
        }

        if(options.empty()){
            ACKPacket ackPacket(0);
            ackPacket.send(sessionSockfd, dst_addr);
            blockNumber = 1;
            sessionState = SessionState::WAITING_DATA;
        } else {
            OACKPacket oackPacket(options);
            oackPacket.send(sessionSockfd, dst_addr);
            blockNumber = 1;
            sessionState = SessionState::WAITING_DATA;
        }
    } else if (sessionType == SessionType::READ){
        std::cout << "Opening file on server: " << src_filename << std::endl;
        this->readStream.open(src_filename, std::ios::binary | std::ios::in);
        if (!readStream.is_open()) {
            ErrorPacket errorPacket(2, "Access violation");
            errorPacket.send(sessionSockfd, dst_addr);
            this->exit();
            return;
        }

        // If "tsize" is set, check if there is enough disk space
        if (options.find("tsize") != options.end()) {
            struct statvfs stat;
            if (statvfs("/", &stat) != 0) {
                // Error occurred getting filesystem stats
                ErrorPacket errorPacket(3, "Disk full or allocation exceeded");
                errorPacket.send(sessionSockfd, dst_addr);
                return;
            }

            uint64_t freeSpace = stat.f_bsize * stat.f_bfree;
            if (freeSpace < options.at("tsize")) {
                // Not enough disk space
                ErrorPacket errorPacket(3, "Disk full or allocation exceeded");
                errorPacket.send(sessionSockfd, dst_addr);
                return;
            }

            // Set the tsize to the actual file size
            this->tsize = std::filesystem::file_size(src_filename);
            options["tsize"] = tsize;
        }

        if (options.empty()){
            std::vector<char> data;
            try {
            data = readDataBlock();
            } catch (const std::runtime_error& e) {
                ErrorPacket errorPacket(3, "Disk full or allocation exceeded");
                errorPacket.send(sessionSockfd, dst_addr);
                this->exit();
                return;
            }
            DataPacket dataPacket(1, data);
            dataPacket.send(sessionSockfd, dst_addr);
            blockNumber++;
            sessionState = SessionState::WAITING_ACK;
        } else {
            OACKPacket oackPacket(options);
            oackPacket.send(sessionSockfd, dst_addr);
            sessionState = SessionState::WAITING_ACK;
        }
    }
    socklen_t dst_len = sizeof(dst_addr);
    while(true){
        if(stopFlag->load()){
            ErrorPacket errorPacket(0, "Server shutdown");
            errorPacket.send(sessionSockfd, dst_addr);
            this->exit();
            return;
        }
        ssize_t n = recvfrom(sessionSockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&dst_addr, &dst_len);
        if (n < 0) {
            std::cout << "Failed to receive data\n";
            this->exit();
            return;
        }

        int srcTID = ntohs(dst_addr.sin_port);
        if (srcTID != this->srcTID){
            ErrorPacket errorPacket(5, "Unknown transfer ID");
            errorPacket.send(sessionSockfd, dst_addr);
        }

        std::unique_ptr<Packet> packet;
        try {
            packet = Packet::parse(dst_addr, buffer, n);
        } catch (const ParsingError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what());
            errorPacket.send(sessionSockfd, dst_addr);
            return;
        }
        catch (const OptionError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what());
            errorPacket.send(sessionSockfd, dst_addr);
            return;
        }
        catch (const std::exception& e) {
            ErrorPacket errorPacket(0, e.what());
            errorPacket.send(sessionSockfd, dst_addr);
            return;
        }

        packet->handleServer(*this);
        if (sessionState == SessionState::WRQ_END || sessionState == SessionState::RRQ_END || sessionState == SessionState::ERROR){
            this->exit();
            return;
        }
    }
}

std::vector<char> ServerSession::readDataBlock() {
    std::vector<char> data(blockSize);
    readStream.read(data.data(), blockSize);
    ssize_t bytesRead = readStream.gcount();
    if (bytesRead <= 0) {
        throw std::runtime_error("Failed to read data from file");
    }

    data.resize(bytesRead);

    return data;
}

void ServerSession::exit(){
    std::cout << "Exiting server session" << std::endl;
    writeStream.close();
    readStream.close();
    close(sessionSockfd);
}


    


