# scripts/profile_server.sh
#!/usr/bin/env bash

## This is the flamegraph generating script

BIN=./build/server
DURATION=10   # seconds

sudo perf record -F 200 -g -- "$BIN" &
PID=$!
echo "Server PID=$PID. Profiling for $DURATION s…"

sleep 5 # sleep long enough to start the server
if [[ "$*" == *"--auto"* ]]; then
    ./test/chat_load_tester 127.0.0.1 8080 1000 10000 256 1 0 testchannel
fi

sleep "$DURATION"
sudo kill -INT "$PID"   # graceful Ctrl-C

mkdir -p ./profiling-data
sudo perf script -i ./perf.data | perl ./external-tools/FlameGraph/stackcollapse-perf.pl | perl ./external-tools/FlameGraph/flamegraph.pl > ./profiling-data/flame.svg
sudo mv perf.data* ./profiling-data/

echo "Flame graph written to ./profiling-data/flame.svg"
