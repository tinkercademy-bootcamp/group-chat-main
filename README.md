# **Directory Structure**
```
epoll-chat/
│
├── src/
│   ├── server/
│   │   ├── channel_manager.cc / channel_manager.h
│   │   ├── epoll-server.cc / epoll-server.h
│   ├── client/
│   │   ├── chat-client.cc / chat-client.h
│   ├── net/
│   │   ├── chat-sockets.cc / chat-sockets.h
│   ├── client-main.cc
│   ├── server-main.cc
│   └── utils.h
├── .gitignore
├── Makefile
├── README.md

```

# Profiling and Stress Testing
Please take a moment to read the first 2 sections of [Flamegraphs] (https://www.brendangregg.com/flamegraphs.html)
 - On first run, use `make setup-flamegraph`. This will clone the FlameGraph repo. Also make the test framework. goto test/ and run `make`.
 - For Stress Testing (including perf and client simulation), run `make stress` in the main folder (after doing `cd ..`).
    - You can change the params of this stress test in auto_profiler.sh
 - To run flamegraph, use `make flamegraph`.
