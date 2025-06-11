CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

all: server client

server: server.cc
	$(CXX) $(CXXFLAGS) -o multicast_server server.cc

client: client.cc
	$(CXX) $(CXXFLAGS) -o multicast_client client.cc

clean:
	rm -f multicast_server multicast_client

.PHONY: all clean
