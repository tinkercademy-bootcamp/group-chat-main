# scripts/profile_server.sh
#!/usr/bin/env bash

## This is the flamegraph generating script

BIN=./build/server
DURATION=${2:-10}   # seconds

sudo perf record -F 997 -g -- "$BIN" & # For reasons that'll take a while to explain, do NOT make it a multiple of 10.
PID=$!
echo "Server PID=$PID. Profiling for $DURATION s…"

sleep 4 # sleep long enough to start the server
if [[ "$*" == *"--auto"* ]]; then
    ./test/chat_load_tester 127.0.0.1 8080 100 100 64 1 10 testchannel
    # Usage: ./test/chat_load_tester <server_ip> <server_port> <num_clients> <messages_per_client> <message_size_bytes> [listen_replies (0 or 1)] [think_time_ms (0+)] [channel_name]
    # Example: ./test/chat_load_tester 127.0.0.1 8080 10 100 64 1 10 testchannel
fi

sleep "$DURATION"
sudo kill -INT "$PID"   # graceful Ctrl-C

mkdir -p ./profiling-data
sudo perf script -i ./perf.data | perl ./external-tools/FlameGraph/stackcollapse-perf.pl | perl ./external-tools/FlameGraph/flamegraph.pl > ./profiling-data/flame.svg
sudo mv perf.data* ./profiling-data/

echo "Flame graph written to ./profiling-data/flame.svg"
