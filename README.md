# C Multithreaded TCP Server

![Language](https://img.shields.io/badge/language-C-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A multithreaded TCP echo server in C (POSIX threads), implemented in **two
architectures** and benchmarked head-to-head:

1. **`multiServ.c`** — thread-per-connection (v1): one detached pthread per client.
2. **`multiServ_pool.c`** — thread pool (v2): fixed worker pool fed by a **bounded
   request queue** (ring buffer + mutex + condition variables), with shared server
   stats protected by a **read-write lock** (any client can send `STATS` for a
   consistent snapshot without serializing readers).

The repo includes a custom multithreaded benchmark client measuring throughput
and RTT percentiles (p50/p95/p99).

## Measured results

Linux, loopback, 128-byte messages, 200 messages/connection
(full details + caveats: [RESULTS.md](RESULTS.md)):

| Concurrency | Thread-per-connection | Thread pool (128 workers) |
|---|---|---|
| 10 | 137k msg/s (p99 0.34 ms) | **601k msg/s (p99 0.03 ms)** |
| 50 | 351k (p99 0.56 ms) | **737k (p99 0.12 ms)** |
| 100 | 563k (p99 0.44 ms) | **698k (p99 0.33 ms)** |
| 200 | 642k (p99 0.64 ms) | **733k (p99 0.31 ms)** |

- Pool wins everywhere: up to **4.4x throughput and ~10x lower p99** at low
  concurrency (warm workers vs. per-connection thread spawn).
- Overload (200 clients vs 64 workers): **481k msg/s, zero failures** — the
  bounded queue blocks the acceptor (backpressure) instead of dropping.
- **ThreadSanitizer: 0 warnings** for both servers under concurrent load.
- Load testing found and fixed a crash: neither server ignored `SIGPIPE`, so a
  client disconnecting mid-write killed the whole process.

## Build & run

```bash
make all            # server (8080), server_pool (8081), benchmark

./server                          # v1: thread-per-connection
./server_pool [port] [workers] [queue_capacity]   # v2: defaults 8081 128 256

# Benchmark either one:
./benchmark -p 8081 -c 100 -n 200 -s 128
#   -c concurrent clients, -n messages/client, -s message size (bytes)

# Race detection (run under load, expect no warnings):
make tsan && ./pool_tsan 8083 32 64 & ./benchmark -p 8083 -c 20 -n 50 -s 128
```

## Design notes

- **Bounded queue**: capacity-limited ring buffer; `queue_push` blocks when full
  so overload back-pressures into the kernel listen backlog rather than growing
  memory or threads without bound.
- **rwlock for stats**: per-message updates take the write lock (nanoseconds
  next to socket syscalls); concurrent `STATS` readers share the read lock.
- **Known ceiling**: workers are connection-granular, so long-lived idle
  connections pin workers. The natural v3 is an event-driven design
  (epoll/io_uring) — this repo deliberately charts the pthread evolution first.
