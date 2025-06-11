all: udp-server tcp-server

#udp-client: udp-test-client.cpp
#	g++ -o udp-client udp-test-client.cpp

udp-server: udp-test-server.cpp
	g++ -o udp-server udp-test-server.cpp

#tcp-client: tcp-test-client.cpp
#	g++ -o tcp-client tcp-test-client.cpp

tcp-server: tcp-test-server.cpp
	g++ -o tcp-server tcp-test-server.cpp

.PHONY: clean

clean:
	rm udp-client udp-server tcp-client tcp-server *.log
