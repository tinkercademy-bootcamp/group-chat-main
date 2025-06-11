# scripts/profile_server.sh
#!/usr/bin/env bash

BIN=./build/server

profile_server() {
    mkdir -p ./profiling-data
    COMMITHASH=$(git rev-parse --short=7 HEAD)

    sudo perf record -F 997 -g -- "$BIN" & # For reasons that'll take a while to explain, do NOT make it a multiple of 10.
    PID=$!
    echo "Server PID=$PID. Profiling for $DURATION s…"

    sleep 4 # sleep long enough to start the server
    ./test/chat_load_tester 127.0.0.1 8080 $num_c $num_m 64 1 10 | tee ./profiling-data/log-${COMMITHASH}-${num_c}-${num_m}.txt
    # Usage: ./test/chat_load_tester <server_ip> <server_port> <num_clients> <messages_per_client> <message_size_bytes> [listen_replies (0 or 1)] [think_time_ms (0+)] [channel_name]
    # Example: ./test/chat_load_tester 127.0.0.1 8080 10 100 64 1 10 testchannel
    sudo kill -INT "$PID"   # graceful Ctrl-C

    FLAMEGRAPH_FILE="./profiling-data/flame-${COMMITHASH}-${num_c}-${num_m}.svg"
    sudo perf script -i ./perf.data | perl ./external-tools/FlameGraph/stackcollapse-perf.pl | perl ./external-tools/FlameGraph/flamegraph.pl > "$FLAMEGRAPH_FILE"
    echo "Flame graph written to $FLAMEGRAPH_FILE"
}


num_m=10
num_c=1
max_iter=20

for ((i=1; i<=max_iter; i++)); do
    echo "Mult is $i (of $iter)"
    profile_server
    num_c=$((num_c * 2))
    profile_server
    num_m=$((num_m * 2))
done
