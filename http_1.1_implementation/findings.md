# HTTP/1.1 Implementation — Code Review Findings

> Review date: 2026-07-20
> Scope: All files under `http_1.1_implementation/`
> Note: This is a learning project — the goal is to identify issues, not fix them.

---

## `server.h` — Server Interface

---
## `server.cpp` — Server Event Loop

### 8. (BUG) `connections.capacity()` used where `size()` is intended
`if (connections.capacity() <= connfd_addr_pair.first)` — `capacity()` is the number of elements the vector can hold without reallocation (can be ≥ `size()`). If the vector already has spare capacity from a previous growth, the resize is skipped even though `connfd_addr_pair.first` is beyond `size()`. Should be `connections.size()`.

### 9. (BUG) `server.run()` is never called
In `main.cpp`, `server.listen()` is called but `server.run()` is not. The program prints the listening address and exits immediately. The server never enters the event loop.

### 10. FD-indexed vector is extremely sparse for high-numbered fds
File descriptors are reused by the OS but can grow large (system limits are often in the hundreds of thousands). Indexing `connections` by raw fd value means accepting a connection on fd 500,000 creates a 500k-element vector (mostly nullptrs). A `std::unordered_map<int, std::unique_ptr<HttpConnection>>` or a fixed-size pool with a separate free list would be more appropriate.

### 11. Compaction threshold counts resets, not gaps
`closed_connections > 1000` triggers compaction. If 1001 connections close, compaction runs once. But if one connection closes at fd 100,000 and no other connection ever opens at that index, the vector stays at size 100,000+ with one entry. There's no mechanism to shrink the vector except hitting the closed_connections threshold again.

### 12. `TCP_NODELAY` set on listening socket instead of accepted connections
`setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, ...)` disables Nagle's algorithm on the listening socket. This should be set on each accepted connection socket instead. While Linux typically inherits socket options on `accept()`, this is not guaranteed across all platforms.

### 13. Potential fd leak if `std::make_unique<HttpConnection>` throws
In the accept handler: if `std::make_unique<HttpConnection>(...)` throws (e.g., out of memory), the accepted `connfd` is never closed. The pair has already been moved from, so the fd is lost.

### 14. `pollfds[0]` hardcoded as listening socket
The code assumes `pollfds[0]` is always the listening socket. This works because it's always emplaced first, but the implicit index coupling makes the code fragile.

### 15. `connections[pfd.fd]` assumes fd is always a valid index
If `poll` returns events for an fd that is not in the `connections` vector (stale fd, bug elsewhere), this is an out-of-bounds access.

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
| **Bug** | 2 | #8 (capacity vs size), #9 (server.run never called) |
| **Design** | 2 | #10 (sparse vector), #12 |
| **Missing** | 2 | #17 (no tests), #18 (no sanitizers) |
| **Minor** | 2 | #13, #14 |
