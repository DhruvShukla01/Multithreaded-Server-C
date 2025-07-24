#!/bin/bash

# Server details
HOST="localhost"
PORT="8080"
NUM_CLIENTS=5
MESSAGE="Hello from client"

# Function to run a single client test
run_client() {
    local client_id=$1
    local output
    
    # Use netcat to send a message and capture the output.
    # The -w1 flag sets a 1-second timeout for the connection.
    output=$(echo "$MESSAGE $client_id" | nc -w 1 $HOST $PORT)

    # The server sends a welcome message and then echoes the input.
    # We check if the echoed message is in the output.
    if echo "$output" | grep -q "$MESSAGE $client_id"; then
        echo "Client $client_id: PASSED"
    else
        echo "Client $client_id: FAILED"
        echo "--- Expected to receive '$MESSAGE $client_id', but got: ---"
        echo "$output"
        echo "--------------------------------------------------------"
    fi
}

echo "--- Starting Concurrent Client Test ---"

# Launch multiple clients in the background
for i in $(seq 1 $NUM_CLIENTS); do
    run_client "$i" &
done

# Wait for all background jobs to finish
wait

echo "--- Test Complete ---"
