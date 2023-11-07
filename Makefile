CXX = g++
CXXFLAGS = -std=c++11 -Wall -Iinclude

all: server client

server: src/server.cpp
	$(CXX) $(CXXFLAGS) -o build/tftp-server src/server.cpp

client: src/client.cpp
	$(CXX) $(CXXFLAGS) -o build/tftp-client src/client.cpp

clean:
	rm -rf build/*