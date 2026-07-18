# HTTP/1.1 Implementation — Code Review Findings

> Review date: 2026-07-18
> Scope: All files under `http_1.1_implementation/`
> Note: This is a learning project — the goal is to identify issues, not fix them.

---

## `http_connection.h` — Data Structures

### 1. `ParsedMessageHeader::headers` — repetitive naming
`conn->req.parsed_headers.headers` reads awkwardly. The field name duplicates the struct name. Consider renaming to `fields` or `header_fields`.

### 2. `RawMessageHeader` used for both requests and responses
Request status lines are formatted as `METHOD /path HTTP/1.1`, while response status lines are `HTTP/1.1 200 OK`. Using the same `RawMessageHeader` struct (with a single `status_line` vector) for both will require manual format-aware parsing later. A separate response status struct may be cleaner.

### 3. `HttpResponse::raw_headers` uses `RawMessageHeader` which has `status_line`
Same as above — response messages don't have a "status line" in the same sense as requests; they have a "status line" that follows a different format. The naming will be confusing.

---

## `http1_parser.h` — Parser Interface

### 4. `encode()` declared but not defined
`static bool encode(HttpConnection *)` is declared in the header but has no definition anywhere. Linking will fail if called.

### 5. `parse_body()` declared but not defined
`static void parse_body(HttpConnection *, std::span<const uint8_t> buf)` is declared but never defined or called from the current `decode()` loop (the `body` case in the switch calls it, so it's a linker error waiting to happen).

---

## `http1_parser.cpp` — Parser Implementation

### 6. (BUG) Case-sensitive header lookups
HTTP header field names are **case-insensitive** per RFC 9110 §5.1. The parsing stores header names as-is (e.g., `Content-Length`, `content-length`, `CONTENT-LENGTH` all end up as different keys in the map). Then `headers.contains("Content-Length")` and `headers.contains("Transfer-Encoding")` do case-sensitive lookups. This will silently miss headers sent with different casing (common in practice).

**Fix direction:** Either lowercase all keys on insert, or use a case-insensitive hash/equality for the `unordered_map`.

### 7. (BUG) Double-free and double-close in move semantics (server.cpp)
In `Server(Server &&)` and `Server::operator=(Server &&)`, the moved-from object's `close()` is called, but `sockfd` and `getaddrinfo_result` are **not reset to -1/nullptr**. When the moved-from `Server` is later destroyed, `~Server()` calls `close()` again on the same (now-stale) values, causing:
  - **Double-free** of `getaddrinfo_result`
  - **Double-close** of `sockfd` (could close a different fd that reused the number)

```cpp
// Move constructor — current:
Server::Server(Server &&s) noexcept
    : sockfd{s.sockfd}, getaddrinfo_result{s.getaddrinfo_result},
      curraddrinfo{s.curraddrinfo} {
  s.close();  // frees s's resources, but s.sockfd and s.getaddrinfo_result are NOT reset
}
// When s is destroyed: ~Server() -> close() again -> double-free!
```

**Fix direction:** After `s.close()`, set `s.sockfd = -1` and `s.getaddrinfo_result = nullptr`. Or use the standard pattern: swap with default-constructed values, then let the destructor handle cleanup.

### 8. `std::ranges::distance(pipeline)` iterates the pipeline twice
In `parse_status_line` (line 96), `std::ranges::distance(pipeline)` walks the entire pipeline to count 3 elements. Then `pipeline.begin()` starts a fresh iteration over the same data. The split/trim/transform work is done twice. The impact is small here (status line is short), but the pattern is wasteful.

**Fix direction:** Materialize the pipeline into a `std::vector` or `std::array` of string_views, then check the size and access elements from the materialized collection.

### 9. `\n`-only split in header parsing
In `parse_headers` (line 153), headers are split on `'\n'` rather than `"\r\n"`. The `\r` gets trimmed by `trim()`, so it works in practice, but:
  - A literal `\n` inside a header value (e.g., in a multi-line `Set-Cookie` or via chunked encoding) would cause an erroneous split.
  - It deviates from the RFC's explicit `CRLF` delimiter.

### 10. Malformed header lines silently dropped
If a line has no colon (line 165: `colon_pos == std::string_view::npos`), it returns `{line, {}}` and the empty-value check `if (v.empty()) continue;` skips it. Malformed headers are silently ignored rather than rejected. Per RFC 9112 §5.1, a malformed header field should result in a `400 Bad Request`.

### 11. Both `from_chars` error branches throw the same message
Lines 194-199: `result_out_of_range` and `invalid_argument` branches both throw identical messages. They can be consolidated into a single check: `if (ec != std::errc{})`.

### 12. Transfer-Encoding only handles `"chunked"` — no fallback
If `Transfer-Encoding` is present but not `"chunked"`, `content_len` stays 0, and the request body is skipped entirely. HTTP/1.1 allows other transfer-codings (e.g., `gzip`, `deflate`, or multiple values like `chunked, gzip`). RFC 9112 §6.1 requires handling unknown transfer-codings with a `501 Not Implemented`.

### 13. Duplicate `Content-Length` is not validated
If multiple `Content-Length` headers with different values are received, the first one is used and the rest are silently ignored. Per RFC 9112 §6.4.2, duplicate `Content-Length` headers with different values must be treated as an error.

### 14. Typo: `"unsupported http vesion"` → `"version"`
Line 111: `"unsupported http vesion.Send 400 Bad request instead of throw."`

### 15. Spurious semicolon on its own line
Line 85 in the staged code has a standalone `;` before the pipeline declaration. Harmless but likely unintentional.

### 16. Error strategy: all failures throw exceptions
Every error case (`throw std::runtime_error(...)`) bypasses the caller's control flow. The runtime errors propagate up to `main()` where they terminate the program. For an HTTP server, errors during parsing should produce HTTP error responses (400, 413, 501), not crash the process. The comments acknowledge this (e.g., "Send 400 Bad request instead of throw").

---

## `server.h` — Server Interface

### 17. `getaddrinfo_result` / `curraddrinfo` are raw owning pointers
These are allocated by `getaddrinfo()` and freed by `freeaddrinfo()` in `close()`. The class relies on manual `close()` calls to free them. If an exception is thrown between construction and destruction, or if `close()` is never called (e.g., after a failed move), memory is leaked. However, the destructor calls `close()`, so this is mostly fine — the only risk is if `close()` throws (it doesn't).

### 18. `curraddrinfo` is stored as class state but only used transiently
`curraddrinfo` is set during `try_bind()` and used once in `listen()` to print the address. It's unnecessary class state — `listen()` could extract the info from the socket or store it locally.

---

## `server.cpp` — Server Event Loop

### 19. (BUG) `connections.capacity()` used where `size()` is intended
Line 206: `if (connections.capacity() <= connfd_addr_pair.first)`
`capacity()` is the number of elements the vector can hold without reallocation (can be ≥ `size()`). If the vector already has spare capacity from a previous growth, you'll skip the resize even though `connfd_addr_pair.first` is beyond `size()`. You almost certainly meant `connections.size()` here.

### 20. (BUG) `server.run()` is never called
In `main.cpp`, `server.listen()` is called but `server.run()` is not. The program prints the listening address and exits immediately. The server never enters the event loop and never handles any connections.

### 21. FD-indexed vector is extremely sparse for high-numbered fds
File descriptors are reused by the OS but can grow large (system limits are often in the hundreds of thousands). Indexing `connections` by raw fd value means that accepting a connection on fd 500,000 creates a 500k-element vector (mostly nullptrs). A `std::unordered_map<int, std::unique_ptr<HttpConnection>>` or a fixed-size pool with a separate free list would be more appropriate.

### 22. Compaction threshold counts resets, not gaps
`closed_connections > 1000` triggers compaction (line 241). If 1001 connections close, compaction runs once. But if one connection closes at fd 100,000 and no other connection ever opens at that index, the vector stays at size 100,000+ with one entry. There's no mechanism to shrink the vector except hitting the closed_connections threshold again.

### 23. No read/write handling implemented
Lines 226 and 230 are empty comments:
```cpp
// handle_read(*conn);
// handle_write(*conn);
```
The event loop spins, polls for events, but never actually reads from or writes to connections. Combined with issue #20, this is dead code.

### 24. `TCP_NODELAY` set on listening socket instead of accepted connections
Line 65: `setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, ...)` — `TCP_NODELAY` disables Nagle's algorithm. This should be set on each accepted connection socket, not the listening socket. While Linux typically inherits socket options on `accept()`, this is not guaranteed across all platforms.

### 25. Potential fd leak if `std::make_unique<HttpConnection>` throws
In the accept handler (lines 204-213): if `std::make_unique<HttpConnection>(...)` throws (e.g., out of memory), the accepted `connfd` is never closed. The pair has already been moved from, so the fd is lost.

### 26. `pollfds[0]` hardcoded as listening socket
The code assumes `pollfds[0]` is always the listening socket. This works because it's always emplaced first, but the implicit index coupling makes the code fragile. A comment or explicit naming would help.

### 27. `connections[pfd.fd]` assumes fd is always a valid index
If `poll` returns events for an fd that for some reason is not in the `connections` vector (race condition, stale fd, etc.), this is an out-of-bounds access. Adding a bounds check would be safer.

---

## `main.cpp` — Entry Point

### 28. `server.run()` not called (duplicate of #20)
The server is constructed, `listen()` is called to bind and start listening, but `run()` is never invoked. The program exits immediately.

### 29. No graceful shutdown handling
There's no way to gracefully stop the server (SIGINT/SIGTERM handling). The `while (true)` loop in `run()` is infinite, and there's no signal handler.

---

## `CMakeLists.txt` — Build Configuration

### 30. No tests defined for the HTTP implementation
Unlike the other two sub-projects (arena, hashtable), this CMakeLists.txt doesn't fetch doctest or define any test targets. The parser logic has no automated tests.

### 31. No sanitizers enabled
The other projects compile tests with `-fsanitize=address,undefined`. This subproject's build targets don't enable any sanitizers.

---

## Summary by Severity

| Severity | Count | Key Issues |
|----------|-------|------------|
| **Bug** | 4 | #6 (case-sensitive headers), #7 (double-free in move), #19 (capacity vs size), #20 (#28 server.run never called) |
| **Design** | 8 | #8 (double pipeline iteration), #9, #10, #12, #13, #16 (error strategy), #21 (sparse vector), #24 |
| **Missing** | 5 | #4 (encode), #5 (parse_body), #23 (read/write), #30 (no tests), #31 (no sanitizers) |
| **Minor** | 4 | #14 (typo), #15 (stray semicolon), #17, #18 |
