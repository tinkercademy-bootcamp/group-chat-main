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

# Notes on Perf and Flamegraph
Please take a moment to read the first 2 sections of https://www.brendangregg.com/flamegraphs.html
To run flamegraph here, run `sh perscr.sh`
It will start the server for 60 seconds. You can stress test it during this time.
After 60 secs, it'll create a flame.svg for you to look into the load profile