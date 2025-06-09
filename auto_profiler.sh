# scripts/profile_server.sh
#!/usr/bin/env bash

BIN=./build/server
DURATION=${1:-10}   # seconds

sudo perf record -F 200 -g -- "$BIN" &
PID=$!
echo "Server PID=$PID. Profiling for $DURATION s…"
sleep "$DURATION"
sudo kill -INT "$PID"   # graceful Ctrl-C

mkdir -p ./profiling
sudo perf script -i ./perf.data | perl ./external-tools/FlameGraph/stackcollapse-perf.pl | flamegraph.pl > ./profiling/flame.svg
sudo mv perf.data* ./profiling-data/

echo "Flame graph written to $(realpath flame.svg)"