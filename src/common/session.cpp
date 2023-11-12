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
        if (sessionState == SessionState::END){
            break;
        }
    }
}

ServerSession::ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {}

void ServerSession::handleSession() {
    char buffer[516];
    if (sessionType == SessionType::WRITE){
        ACKPacket ackPacket(0);
        ackPacket.send(sessionSockfd, dst_addr);
        blockNumber = 1;
        sessionState = SessionState::WAITING_DATA;
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
    }
}


    


