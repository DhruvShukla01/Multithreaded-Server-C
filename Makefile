# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

# Target executables
SERVER_TARGET = server
BENCHMARK_TARGET = benchmark

# Source files
SERVER_SRC = multiServ.c
BENCHMARK_SRC = benchmark.c

# Phony targets are rules that don't produce an output file with the same name.
.PHONY: all run test run-benchmark clean

# Default target: builds both the server and the benchmark tool.
all: $(SERVER_TARGET) $(BENCHMARK_TARGET)

# Rule to build the server executable.
$(SERVER_TARGET): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_SRC)

# Rule to build the benchmark tool executable.
# NOTE: The TARGET here is 'benchmark', the file name.
$(BENCHMARK_TARGET): $(BENCHMARK_SRC)
	$(CC) $(CFLAGS) -o $(BENCHMARK_TARGET) $(BENCHMARK_SRC)

# Rule to run the server interactively.
run: $(SERVER_TARGET)
	./$(SERVER_TARGET)

# Rule to run the simple functional test.
# It depends on the server executable being built first.
test: $(SERVER_TARGET)
	@echo "--- Running Basic Functional Test ---"
	@./$(SERVER_TARGET) & SERVER_PID=$$!; \
	echo "Server started with PID: $${SERVER_PID}"; \
	trap "echo 'Stopping server...'; kill $${SERVER_PID}" EXIT; \
	sleep 1; \
	echo "Running test script..."; \
	./test_server.sh; \
	echo "Tests finished.";

# Rule to run the performance benchmark.
# NOTE: This target is RENAMED to 'run-benchmark' to avoid conflict.
# It depends on BOTH executables being built.
# Using a single shell command sequence ensures SERVER_PID is available to trap.
run-benchmark: $(SERVER_TARGET) $(BENCHMARK_TARGET)
	@echo "--- Running Performance Benchmark ---"
	@./$(SERVER_TARGET) & SERVER_PID=$$!; \
	echo "Server started with PID: $${SERVER_PID}"; \
	trap "echo; echo 'Cleaning up and stopping server...'; kill $${SERVER_PID}" EXIT; \
	sleep 1; \
	echo "Running benchmark..."; \
	./$(BENCHMARK_TARGET) -c 100 -n 1000 -s 256; \
	echo "Benchmark finished.";

# Rule to clean up all build artifacts.
clean:
	@echo "Cleaning up build artifacts..."
	rm -f $(SERVER_TARGET) $(BENCHMARK_TARGET)