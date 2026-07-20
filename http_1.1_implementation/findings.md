# HTTP/1.1 Implementation — Code Review Findings

> Review date: 2026-07-20
> Scope: All files under `http_1.1_implementation/`
> Note: This is a learning project — the goal is to identify issues, not fix them.

---

## `server.h` — Server Interface

---
## `server.cpp` — Server Event Loop

### 9. (BUG) `server.run()` is never called
In `main.cpp`, `server.listen()` is called but `server.run()` is not. The program prints the listening address and exits immediately. The server never enters the event loop.

### 12. `TCP_NODELAY` set on listening socket instead of accepted connections
`setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, ...)` disables Nagle's algorithm on the listening socket. This should be set on each accepted connection socket instead. While Linux typically inherits socket options on `accept()`, this is not guaranteed across all platforms.

### 13. Potential fd leak if `std::make_unique<HttpConnection>` throws
In the accept handler: if `std::make_unique<HttpConnection>(...)` throws (e.g., out of memory), the accepted `connfd` is never closed. The fd is still held in `connfd_addr_pair`, but that's a local `pair<int, ...>` whose destructor is trivial — it won't close the fd.

---

## `main.cpp` — Entry Point

### 16. No graceful shutdown handling
There's no way to gracefully stop the server (SIGINT/SIGTERM handling). The `while (true)` loop in `run()` is infinite, and there's no signal handler.

---

## `CMakeLists.txt` — Build Configuration

### 17. No tests defined for the HTTP implementation
Unlike the other two sub-projects (arena, hashtable), this CMakeLists.txt doesn't fetch doctest or define any test targets. The parser logic has no automated tests.

### 18. No sanitizers enabled
The other projects compile tests with `-fsanitize=address,undefined`. This subproject's build targets don't enable any sanitizers.

---

## Summary by Severity

| Severity | Count | Key Issues |
|----------|-------|------------|
| **Bug** | 1 | #9 (server.run never called) |
| **Design** | 1 | #12 |
| **Missing** | 2 | #17 (no tests), #18 (no sanitizers) |
| **Minor** | 1 | #13 |
