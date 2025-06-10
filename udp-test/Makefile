all: client server

client: udp-test-client.cpp
	g++ -o $@ $<

server: udp-test-server.cpp
	g++ -o $@ $<

.PHONY: clean

clean:
	rm client server