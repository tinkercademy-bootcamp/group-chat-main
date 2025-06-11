#!/bin/bash

echo "Multicast UDP Test Script"
echo "========================"

# Compile the programs
echo "Compiling programs..."
make clean
make

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Programs compiled successfully!"
echo ""
echo "To test the multicast functionality:"
echo ""
echo "1. In one terminal, start the server:"
echo "   ./multicast_server"
echo ""
echo "2. In separate terminals, start multiple clients:"
echo "   ./multicast_client Client1"
echo "   ./multicast_client Client2"
echo "   ./multicast_client Client3"
echo ""
echo "You should see all clients receive the same messages from the server."
echo ""
echo "Press any key to start a demo with background processes..."
read -n 1 -s

echo "Starting server in background..."
./multicast_server &
SERVER_PID=$!

sleep 2

echo "Starting 3 clients in background..."
./multicast_client "Client-A" &
CLIENT1_PID=$!

./multicast_client "Client-B" &
CLIENT2_PID=$!

./multicast_client "Client-C" &
CLIENT3_PID=$!

echo ""
echo "Demo running... Press Enter to stop all processes"
read

echo "Stopping all processes..."
kill $SERVER_PID $CLIENT1_PID $CLIENT2_PID $CLIENT3_PID 2>/dev/null

echo "Demo completed!"