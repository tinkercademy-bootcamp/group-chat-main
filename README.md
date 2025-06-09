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

# Flamegraph and Perf
!IMPORTANT
Please take a moment to read the first 2 sections of [Flamegraphs] (https://www.brendangregg.com/flamegraphs.html)
 - On first run, use `make setup-flamegraph`. This will clone the FlameGraph repo.
 - To run flamegraph, use `make flamegraph`.


 - It will start the server for 60 seconds. You can stress test it during this time.
 - After 60 secs, it'll create profiling-data/flame.svg for you to look into the load profile. Open it in a web browser for better readability.
 - You can customise this duration in auto_profiler.sh.
