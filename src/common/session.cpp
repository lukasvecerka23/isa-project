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

ClientSession::ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {}

void ClientSession::handleSession() {
    if (sessionType == SessionType::READ){
        std::cout << "Opening file on server: " << dst_filename << std::endl;
        this->writeStream.open(dst_filename, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!writeStream.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        blockNumber = 1;
        sessionState = SessionState::INITIAL;
    }
    char buffer[516];
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

ServerSession::ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {}

void ServerSession::handleSession() {

    char buffer[516];
    if (sessionType == SessionType::WRITE){
        std::cout << "Opening file on server: " << dst_filename << std::endl;
        this->writeStream.open(dst_filename, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!writeStream.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        ACKPacket ackPacket(0);
        ackPacket.send(sessionSockfd, dst_addr);
        blockNumber = 1;
        sessionState = SessionState::WAITING_DATA;
    } else if (sessionType == SessionType::READ){
        std::cout << "Opening file on server: " << src_filename << std::endl;
        this->readStream.open(src_filename, std::ios::binary | std::ios::in);
        if (!readStream.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        DataPacket dataPacket(1, readDataBlock());
        dataPacket.send(sessionSockfd, dst_addr);
        blockNumber++;
        sessionState = SessionState::WAITING_ACK;
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


    


