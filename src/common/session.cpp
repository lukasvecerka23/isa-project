#include "common/session.hpp"
#include "common/packets.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>

Session::Session(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType) {
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

Session::~Session() {
    close(sessionSockfd);
}

ClientSession::ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, int> options)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {
        this->options = options;
    }

void ClientSession::handleSession() {
    if (sessionType == SessionType::READ){
        std::cout << "Opening file on client: " << dst_filename << std::endl;
        this->writeStream.open(dst_filename, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!writeStream.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        blockNumber = 1;
    }
    if (!options.empty()){
        sessionState = SessionState::WAITING_OACK;
    }
    char buffer[BUFFER_SIZE];
    socklen_t dst_len = sizeof(dst_addr);
    while(true){
        ssize_t n = recvfrom(sessionSockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&dst_addr, &dst_len);
        if (n < 0) {
            std::cerr << "Failed to receive data\n";
            continue; // In a real server you might want to handle this differently
        }
        std::unique_ptr<Packet> packet = Packet::parse(dst_addr, buffer, n);

        packet->handleClient(*this);
        if (sessionState == SessionState::RRQ_END || sessionState == SessionState::WRQ_END){
            break;
        }
    }
}

std::vector<char> ClientSession::readDataBlock() {
    
    std::vector<char> data(blockSize);
    std::cin.read(data.data(), blockSize);
    ssize_t bytesRead = std::cin.gcount();
    
    if (bytesRead <= 0) {
        throw std::runtime_error("Failed to read data from stdin");
    }

    data.resize(bytesRead);

    return data;
}

void ClientSession::setOptions(std::map<std::string, int> newOptions){
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

ServerSession::ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, int> options)
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
            throw std::runtime_error("Failed to open file");
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
            throw std::runtime_error("Failed to open file");
        }
        if (options.empty()){
            DataPacket dataPacket(1, readDataBlock());
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
        ssize_t n = recvfrom(sessionSockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&dst_addr, &dst_len);
        if (n < 0) {
            std::cerr << "Failed to receive data\n";
            continue; // In a real server you might want to handle this differently
        }
        std::unique_ptr<Packet> packet = Packet::parse(dst_addr, buffer, n);

        packet->handleServer(*this);
        if (sessionState == SessionState::WRQ_END || sessionState == SessionState::RRQ_END){
            break;
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


    


