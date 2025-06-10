CXX = g++
CXXFLAGS = -std=c++20

CLIENT_SRC = udp-client.cc
CLIENT_BIN = client

SERVER_SRC = udp-server.cc
SERVER_BIN = server

.PHONY: all clean

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)
	