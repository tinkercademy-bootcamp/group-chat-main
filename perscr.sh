# scripts/profile_server.sh
#!/usr/bin/env bash
set -euo pipefail

BIN=./build/server
DURATION=${1:-120}   # seconds

sudo perf record -F 200 -g -- "$BIN" &
PID=$!
echo "Server PID=$PID. Profiling for $DURATION s…"
sleep "$DURATION"
sudo kill -INT "$PID"   # graceful Ctrl-C

sudo perf script | perl ./tools/stackcollapse-perf.pl | flamegraph.pl > flame.svg
echo "Flame graph written to $(realpath flame.svg)"