# Benchmark Results — 2026-07-22

Two architectures, same echo protocol, measured with the repo's own multithreaded
benchmark client (extended with RTT p50/p95/p99). Environment: Linux VM, loopback,
128-byte messages, 200 messages/connection. Numbers are messages/sec.

| Concurrency | Thread-per-connection (multiServ.c) | Thread pool, 128 workers (multiServ_pool.c) |
|---|---|---|
| 10  | 137k (p99 0.34 ms) | **601k (p99 0.03 ms)** |
| 50  | 351k (p99 0.56 ms) | **737k (p99 0.12 ms)** |
| 100 | 563k (p99 0.44 ms) | **698k (p99 0.33 ms)** |
| 200 | 642k (p99 0.64 ms) | **733k (p99 0.31 ms)** |

Overload behavior: pool with 64 workers vs 200 concurrent clients (3x
oversubscription) still sustained **481k msgs/sec, p99 0.39 ms** — the bounded
queue applies backpressure at the acceptor instead of failing.

ThreadSanitizer (`make tsan`): **0 warnings** for both servers under concurrent
load (20–40 clients).

## Architecture (multiServ_pool.c)

- Fixed worker pool (default 128) — no per-connection pthread_create, no
  unbounded thread growth.
- Bounded request queue: ring buffer + mutex + not_empty/not_full condvars;
  acceptor blocks when full (backpressure).
- Shared stats (connections/messages/bytes) behind a `pthread_rwlock_t`:
  writers update per message, clients can send `STATS` to read a consistent
  snapshot concurrently.

## Bug found by load testing (fixed in both servers)

Neither server ignored `SIGPIPE`: a `write()` to a socket whose client
disconnected mid-exchange terminated the entire process. Under benchmark load
this killed the server within seconds. Fixed with `signal(SIGPIPE, SIG_IGN)`.

## Caveats

Loopback (no NIC), trivial echo workload, results scale with core count.
Relative comparison (pool vs thread-per-conn) is the robust claim; absolute
msgs/sec depends on hardware.
