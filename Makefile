CXX := g++
CXXFLAGS := -std=c++20 -Wall -Iinclude
LDFLAGS :=

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build

CLIENT_TARGET := tftp-client
SERVER_TARGET := tftp-server

# Get source files using wildcard
COMMON_SRC := $(wildcard $(SRC_DIR)/common/*.cpp)
CLIENT_SRC := $(wildcard $(SRC_DIR)/client/*.cpp)
SERVER_SRC := $(wildcard $(SRC_DIR)/server/*.cpp)

# Replace .cpp with .o in the source file paths
COMMON_OBJ := $(COMMON_SRC:$(SRC_DIR)/common/%.cpp=$(BUILD_DIR)/common/%.o)
CLIENT_OBJ := $(CLIENT_SRC:$(SRC_DIR)/client/%.cpp=$(BUILD_DIR)/client/%.o)
SERVER_OBJ := $(SERVER_SRC:$(SRC_DIR)/server/%.cpp=$(BUILD_DIR)/server/%.o)

.PHONY: all clean client server

all: client server

run_server: server
	./$(SERVER_TARGET) ./server_dir

run_client_download: client
	./$(CLIENT_TARGET) -h localhost -p 69 -f $(SOURCE) -t $(DESTINATION)

run_client_upload: client
	./$(CLIENT_TARGET) -h localhost -p 69 -t $(DESTINATION) < $(SOURCE)

client: $(CLIENT_TARGET)

server: $(SERVER_TARGET)

$(CLIENT_TARGET): $(COMMON_OBJ) $(CLIENT_OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@

$(SERVER_TARGET): $(COMMON_OBJ) $(SERVER_OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/common/%.o: $(SRC_DIR)/common/%.cpp | $(BUILD_DIR)/common
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/client/%.o: $(SRC_DIR)/client/%.cpp | $(BUILD_DIR)/client
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/server/%.o: $(SRC_DIR)/server/%.cpp | $(BUILD_DIR)/server
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/common $(BUILD_DIR)/client $(BUILD_DIR)/server:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

archive:
	tar -cvf xvecer30.tar src include Makefile README manual.pdf test_tftp.py