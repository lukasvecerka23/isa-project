CXX := g++
CXXFLAGS := -std=c++14 -Wall -Iinclude
LDFLAGS :=

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build

CLIENT_TARGET := $(BUILD_DIR)/client/tftp-client
SERVER_TARGET := $(BUILD_DIR)/server/tftp-server

COMMON_SRC := $(wildcard $(SRC_DIR)/common/*.cpp)
COMMON_OBJ := $(patsubst $(SRC_DIR)/common/%.cpp,$(BUILD_DIR)/common/%.o,$(COMMON_SRC))

CLIENT_SRC := $(wildcard $(SRC_DIR)/client/*.cpp)
CLIENT_OBJ := $(patsubst $(SRC_DIR)/client/%.cpp,$(BUILD_DIR)/client/%.o,$(CLIENT_SRC))

SERVER_SRC := $(wildcard $(SRC_DIR)/server/*.cpp)
SERVER_OBJ := $(patsubst $(SRC_DIR)/server/%.cpp,$(BUILD_DIR)/server/%.o,$(SERVER_SRC))

.PHONY: all clean client server

all: client server

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