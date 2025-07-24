# C Multithreaded TCP Server

![Language](https://img.shields.io/badge/language-C-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A high-performance, multithreaded TCP echo server written in C using POSIX Threads (Pthreads). This project demonstrates core concepts of network programming, concurrency, and performance benchmarking in a Unix-like environment.

The repository includes both the server and a custom multithreaded benchmark client used to stress-test the server and measure key performance metrics.

***

## Features

* **Multithreaded Architecture:** Uses a one-thread-per-connection model to handle multiple clients concurrently.
* **Robust I/O Handling:** Correctly manages network socket parameters (`listen` backlog) and client-server synchronization to prevent deadlocks and connection drops under load.
* **Custom Benchmark Tool:** A powerful benchmark client (`benchmark.c`) that uses Pthreads to generate configurable concurrent loads for stress testing.
* **Makefile Automation:** Includes a comprehensive `Makefile` for easy compilation, testing, and execution of both the server and client.
* **Performance Metrics:** The benchmark tool provides key metrics, including throughput (messages/sec), data rate (MB/s), and total transaction time.

***

## Getting Started

### Prerequisites

* A C compiler like `gcc`
* `make` build automation tool
* A Unix-like operating system (Linux, macOS)

### Installation & Usage

1.  **Clone the repository:**
    ```bash
    git clone <your-repository-url>
    cd <repository-directory>
    ```

2.  **Compile the project:**
    The `Makefile` will compile both the `server` and the `benchmark` executables.
    ```bash
    make all
    ```

3.  **Run the Server:**
    To start the server and have it listen for connections on port 8080:
    ```bash
    ./server
    # Or use the make rule
    make run
    ```

4.  **Run the Tests:**

    * **Functional Test:** This runs a simple script with 5 clients to verify basic server functionality.
        ```bash
        make test
        ```
    * **Performance Benchmark:** This runs the full benchmark with 100 concurrent clients.
        ```bash
        make run-benchmark
        ```

5.  **Clean Up:**
    To remove the compiled executables:
    ```bash
    make clean
    ```

***

## Performance

The server was optimized to resolve critical deadlock and resource allocation bugs found in the initial implementation. The final version successfully passed a rigorous benchmark without errors, demonstrating stability and high throughput.

**Benchmark Parameters:**
* **Concurrent Clients:** 100
* **Messages per Client:** 1,000
* **Message Size:** 256 bytes

**Benchmark Results (Localhost):**
| Metric             | Result                  |
| :----------------- | :---------------------- |
| **Total Messages** | 100,000                 |
| **Throughput** | **~34,600** messages/sec |
| **Data Rate** | **8.44 MB/s** |
| **Avg. Latency** | ~29 µs / message        |
| **Test Duration** | 2.89 seconds            |

***

## Architecture & Future Improvements

The current architecture uses a **one-thread-per-connection** model. While simple and effective for a moderate number of connections, its primary drawback is the overhead of creating a new thread for every client.

Future work will focus on implementing more advanced, scalable architectures:

1.  **Thread Pool:** Transition to a thread pool to reuse a fixed number of worker threads. This will significantly reduce CPU and memory overhead, boosting performance for short-lived connections.
2.  **Event-Driven I/O (`epoll` / `kqueue`):** For ultimate scalability, the server can be re-architected to use an event-driven model with `epoll` (Linux) or `kqueue` (macOS/BSD). This would allow a single thread to manage thousands of concurrent connections efficiently.

***

## License

This project is licensed under the MIT License.
