#include "common/session.hpp"
#include "common/packets.hpp"
#include "common/exceptions.hpp"
#include "common/logger.hpp"
#include <sys/statvfs.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>

// Define stopFlag
std::shared_ptr<std::atomic<bool>> stopFlagServer = std::make_shared<std::atomic<bool>>(false);
std::shared_ptr<std::atomic<bool>> stopFlagClient = std::make_shared<std::atomic<bool>>(false);

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

bool hasEnoughSpace(uint64_t size){
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) {
        // Error occurred getting filesystem stats
        return false;
    }

    uint64_t freeSpace = stat.f_bsize * stat.f_bfree;
    return freeSpace >= size;
}

Session::Session(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType)
: dst_addr(dst_addr),
srcTID(ntohs(dst_addr.sin_port)),
sessionSockfd(socket),
blockNumber(0),
blockSize(INITIAL_BLOCK_SIZE),
timeout(INITIAL_TIMEOUT),
initialTimeout(INITIAL_TIMEOUT),
tsize(INITIAL_TSIZE),
dataMode(dataMode),
sessionType(sessionType),
src_filename(src_filename),
dst_filename(dst_filename),
sessionState(SessionState::INITIAL),
fileOpen(false),
retries(0),
lastPacket(nullptr)
{}



void Session::setTimeout(){
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(sessionSockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        Logger::instance().log("Failed to set timeout");
        return;
    }
}

bool Session::openFileForWrite(){
    Logger::instance().log("Opening file on server: " + dst_filename);
    this->writeStream.open(dst_filename, std::ios::binary | std::ios::trunc | std::ios::out);
    if (!writeStream.is_open()) {
        return false;
    } else {
        fileOpen = true;
    }
    return true;
}

ClientSession::ClientSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, uint64_t> options)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {
        this->options = options;
        this->TIDisSet = false;
    }

void ClientSession::handleSession() {
    
    if (sessionType == SessionType::READ){
        if (!openFileForWrite()){
            Logger::instance().log("Failed to open file for writing");
            sessionState = SessionState::ERROR;
            this->exit();
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
        // SIGINT termination
        if(stopFlagClient->load()){
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }

        setTimeout();
        // Receive data from server
        ssize_t received_bytes = recvfrom(sessionSockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&dst_addr, &dst_len);


        if (received_bytes < 0) {
            // Timeouted
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Check if the number of retries is exceeded
                if (++retries > MAX_RETRIES) {
                    Logger::instance().log("Max retries reached, giving up.");
                    sessionState = SessionState::ERROR;
                    this->exit();
                    return;
                }

                Logger::instance().log("Timeout, retransmitting (attempt " + std::to_string(retries) + ").");

                // Retransmit the last packet
                lastPacket->send(this, sessionSockfd);

                // Implement exponential backoff
                timeout *= BACKOFF_FACTOR;
                continue;
            } else {
                Logger::instance().log("Failed to receive data");
                sessionState = SessionState::ERROR;
                this->exit();
                return;
            }
        }
        // Reset the number of retries
        retries = 0;
        timeout = initialTimeout;

        // First packet received, set the TID
        if (!TIDisSet){
            this->srcTID = ntohs(dst_addr.sin_port);
            TIDisSet = true;
        }

        // Check if the TID matches
        int srcTID = ntohs(dst_addr.sin_port);
        if (srcTID != this->srcTID){
            ErrorPacket errorPacket(5, "Unknown transfer ID", dst_addr);
            errorPacket.send(this, sessionSockfd);
            continue;
        }

        // Try to parse the packet
        std::unique_ptr<Packet> packet;
        try {
            packet = Packet::parse(dst_addr, buffer, received_bytes);
        } catch (const ParsingError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what(), dst_addr);
            errorPacket.send(this, sessionSockfd);
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }
        catch (const OptionError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what(), dst_addr);
            errorPacket.send(this, sessionSockfd);
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }
        catch (const std::exception& e) {
            ErrorPacket errorPacket(0, e.what(), dst_addr);
            errorPacket.send(this, sessionSockfd);
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }

        // hanndle the packet
        packet->handleClient(this);

        // Check if the session is finished
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
        Logger::instance().log("Setting block size to " + std::to_string(options.at("blksize")));
        this->blockSize = options.at("blksize");
    }

    // Check if the options map contains the "timeout" option
    if (options.find("timeout") != options.end()) {
        // Set the timeout to its value
        Logger::instance().log("Setting timeout to " + std::to_string(options.at("timeout")));
        this->initialTimeout = options.at("timeout");
        this->timeout = options.at("timeout");
    }

    // Check if the options map contains the "tsize" option
    if (options.find("tsize") != options.end()) {
        // Set the tsize to its value
        Logger::instance().log("Setting tsize to " + std::to_string(options.at("tsize")));
        this->tsize = options.at("tsize");
    }
}

void ClientSession::exit(){
    if (sessionState == SessionState::ERROR && fileOpen && sessionType == SessionType::READ) {
        Logger::instance().log("File was not correctly transfered, deleting file...");
        if (std::remove(dst_filename.c_str())){
            Logger::instance().log("Failed to delete file");
        } else {
            Logger::instance().log("File deleted");
        }
    }
    Logger::instance().log("Exiting client session");
    writeStream.close();
    close(sessionSockfd);
}

ServerSession::ServerSession(int socket, const sockaddr_in& dst_addr, const std::string src_filename, const std::string dst_filename, DataMode dataMode, SessionType sessionType, std::map<std::string, uint64_t> options)
    : Session(socket, dst_addr, src_filename, dst_filename, dataMode, sessionType) {
        this->options = options;
    }

void ServerSession::handleSession() {
    char buffer[BUFFER_SIZE];
    if (sessionType == SessionType::WRITE){
        if (!handleWriteRequest()){
            Logger::instance().log("Failed to handle write request");
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }

    } else if (sessionType == SessionType::READ){
        if (!handleReadRequest()){
            Logger::instance().log("Failed to handle read request");
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }
    }

    socklen_t dst_len = sizeof(dst_addr);
    while(true){
        // SIGINT termination
        if(stopFlagServer->load()){
            ErrorPacket errorPacket(0, "Server shutdown", dst_addr);
            errorPacket.send(this, sessionSockfd);
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }

        setTimeout();
        // Receive data from client
        ssize_t received_bytes = recvfrom(sessionSockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&dst_addr, &dst_len);

        if (received_bytes < 0) {
            // Timeouted
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Check if the number of retries is exceeded
                if (++retries > 3) {
                    Logger::instance().log("Max retries reached, giving up.");
                    sessionState = SessionState::ERROR;
                    this->exit();
                    return;
                }

                Logger::instance().log("Timeout, retransmitting (attempt " + std::to_string(retries) + ").");

                lastPacket->send(this, sessionSockfd);

                // Implement exponential backoff
                timeout *= 2;
                continue;
            } else {
                Logger::instance().log("Failed to receive data");
                sessionState = SessionState::ERROR;
                this->exit();
                return;
            }
        }
        // Reset the number of retries
        retries = 0;
        timeout = initialTimeout;

        // Check if the TID matches
        int srcTID = ntohs(dst_addr.sin_port);
        if (srcTID != this->srcTID){
            ErrorPacket errorPacket(5, "Unknown transfer ID", dst_addr);
            errorPacket.send(this, sessionSockfd);
            continue;
        }

        // Try to parse the packet
        std::unique_ptr<Packet> packet;
        try {
            packet = Packet::parse(dst_addr, buffer, received_bytes);
        } catch (const ParsingError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what(), dst_addr);
            errorPacket.send(this, sessionSockfd);
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }
        catch (const OptionError& e) {
            ErrorPacket errorPacket(ParsingError::errorCode, e.what(), dst_addr);
            errorPacket.send(this, sessionSockfd);
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }
        catch (const std::exception& e) {
            ErrorPacket errorPacket(0, e.what(), dst_addr);
            errorPacket.send(this, sessionSockfd);
            sessionState = SessionState::ERROR;
            this->exit();
            return;
        }

        // handle the packet
        packet->handleServer(this);

        // Check if the session is finished
        if (sessionState == SessionState::WRQ_END || sessionState == SessionState::RRQ_END || sessionState == SessionState::ERROR){
            this->exit();
            return;
        }
    }
}

bool ServerSession::handleWriteRequest(){
    // If "tsize" is set, check if there is enough disk space
    if (options.find("tsize") != options.end()) {
        if (!hasEnoughSpace(options.at("tsize"))){
            // Not enough disk space
            ErrorPacket errorPacket(3, "Disk full or allocation exceeded", dst_addr);
            errorPacket.send(this, sessionSockfd);
            return false;
        }
    }

    if (!openFileForWrite()) {
        ErrorPacket errorPacket(2, "Access violation", dst_addr);
        errorPacket.send(this, sessionSockfd);
        return false;
    }


    if(options.empty()){
        ACKPacket ackPacket(0, dst_addr);
        ackPacket.send(this, sessionSockfd);
        blockNumber = 1;
        sessionState = SessionState::WAITING_DATA;
    } else {
        OACKPacket oackPacket(options, dst_addr);
        oackPacket.send(this, sessionSockfd);
        blockNumber = 1;
        sessionState = SessionState::WAITING_AFTER_OACK;
    }
    return true;
}

bool ServerSession::openFileForRead(){
    Logger::instance().log("Opening file on server: " + src_filename);
    this->readStream.open(src_filename, std::ios::binary | std::ios::in);
    if (!readStream.is_open()) {
        return false;
    } else {
        fileOpen = true;
    }
    return true;
}

bool ServerSession::handleReadRequest(){
        if (options.find("tsize") != options.end()) {
            // Set the tsize to the actual file size
            this->tsize = std::filesystem::file_size(src_filename);
            options["tsize"] = tsize;
        }

        if (!openFileForRead()) {
            ErrorPacket errorPacket(2, "Access violation", dst_addr);
            errorPacket.send(this, sessionSockfd);
            return false;
        }
        
        if (options.empty()){
            std::vector<char> data;
            try {
            data = readDataBlock();
            } 
            catch (const std::runtime_error& e) {
                ErrorPacket errorPacket(3, "Disk full or allocation exceeded", dst_addr);
                errorPacket.send(this, sessionSockfd);
                return false;
            }
            DataPacket dataPacket(1, data, dst_addr);
            dataPacket.send(this, sessionSockfd);
            blockNumber++;
            if (data.size() < blockSize){
                sessionState = SessionState::WAITING_LAST_ACK;
            } else {
                sessionState = SessionState::WAITING_ACK;
            }
        } else {
            OACKPacket oackPacket(options, dst_addr);
            oackPacket.send(this, sessionSockfd);
            sessionState = SessionState::WAITING_AFTER_OACK;
        }
        return true;
}

void ServerSession::setOptions(){
        // Check if the options map contains the "blksize" option
    if (options.find("blksize") != options.end()) {
        // Set the blockSize member variable to its value
        Logger::instance().log("Setting block size to " + std::to_string(options.at("blksize")));
        this->blockSize = options.at("blksize");
    }

    // Check if the options map contains the "timeout" option
    if (options.find("timeout") != options.end()) {
        // Set the timeout to its value
        Logger::instance().log("Setting timeout to " + std::to_string(options.at("timeout")));
        this->initialTimeout = options.at("timeout");
        this->timeout = options.at("timeout");
    }

    // Check if the options map contains the "tsize" option
    if (options.find("tsize") != options.end()) {
        // Set the tsize to its value
        Logger::instance().log("Setting tsize to " + std::to_string(options.at("tsize")));
        this->tsize = options.at("tsize");
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
    Logger::instance().log("Exiting server session");
    if (sessionState == SessionState::ERROR && fileOpen && sessionType == SessionType::WRITE) {
        Logger::instance().log("File was not correctly transfered, deleting file...");
        if (std::remove(dst_filename.c_str())){
            Logger::instance().log("Failed to delete file");
        } else {
            Logger::instance().log("File deleted");
        }
    }
    writeStream.close();
    readStream.close();
    close(sessionSockfd);
}
