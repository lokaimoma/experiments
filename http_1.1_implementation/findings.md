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


---

## `http1_parser.cpp` — Parser

### 20. Body data after `\r\n\r\n` bypasses `body_len` overflow check
In `read_headers` (`http1_parser.cpp:169-173`), data remaining in the buffer after the header boundary is inserted directly into `conn.req.body` without checking whether it exceeds `body_len`. The overflow check in `read_body` is only reached on the *next* `decode()` call, so the body can over-allocate before the error fires.

### 21. Chunked Transfer-Encoding not decoded
For `TE: chunked`, `body_len` is set to `MAX_BODY_LEN` and raw bytes are dumped verbatim into `conn.req.body`. Chunk-size hex lines, CRLFs, and trailing headers all become part of the body data. There is no dechunking loop, and `body_len` conflates "expected decoded size" with "max raw-read limit."

### 22. Transfer-Encoding: chunked must be the final coding
RFC 7230 §3.3.1 requires chunked to be the last transfer coding (e.g. `TE: gzip, chunked` is valid, `TE: chunked, gzip` is not). The code only checks whether `"chunked"` is a substring of the first TE value — it doesn't validate ordering or reject invalid combinations.

---

## `http1_parser.h` — Parser Interface

### 23. `parse_body` declared but never defined
`static void parse_body(HttpConnection &)` is declared as a private static method (`http1_parser.h:20`) but has no definition. Dead declaration.

---

## `http_connection.h` — Data Structures

### 24. `body_encoding` field never populated
`HttpRequest::body_encoding` (`http_connection.h:28`) is declared as `std::optional<std::string>` but never set. The comment `// set req.body_encoding here` in the chunked branch (`http1_parser.cpp:226`) is a no-op. Moreover, `body_encoding` should represent *content* encoding (gzip, br), not *transfer* encoding (chunked) — the naming and placement conflate the two concepts.

---

## General — Cross-cutting Concerns

### 25. No Content-Encoding decompression
gzip, br, deflate, and compress are never decoded. A client sending `Content-Encoding: gzip` with a gzip-compressed body will have raw compressed bytes stored as the body.

### 26. No `Expect: 100-continue` handling
Per RFC 9110 §10.1.1, a client sending `Expect: 100-continue` expects a `100 Continue` response before sending the body. The server currently reads the body unconditionally, and never sends `100 Continue` or `417 Expectation Failed`.

### 27. All errors thrown as exceptions, never sent as HTTP responses
Every validation failure (`400`, `411`, `413`, `501`) throws `std::runtime_error`, which propagates up and terminates the connection (or the program). No error ever produces an actual HTTP response to the client.

### 28. `MAX_BODY_LEN` (16 KiB) is arbitrary and hardcoded
No configurable limit. A legitimate POST with a larger body is rejected at 16 KiB regardless of available resources.

### 29. Request target / URI not validated
The parsed `req.path` is taken verbatim from the request line with no validation for origin-form, absolute-form, authority-form, or asterisk-form. No path normalization or security sanitization.

### 30. HTTP pipelining not supported
Multiple requests arriving on a single connection are not handled. The parser stops after the first complete request-response cycle.

### 31. Connection persistence not handled
No `Connection: keep-alive` vs `close` handling. All connections have the same lifecycle, and there's no logic to reuse a connection for subsequent requests.

### 32. Response generation (`encode`) unimplemented
`Http1Parser::encode()` is declared (returns `bool`) but has no visible implementation. No HTTP responses are ever constructed or sent.

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
| **Bug** | 2 | #9 (server.run never called), #20 (body data bypasses overflow check) |
| **Missing** | 15 | #16 (no graceful shutdown), #17 (no tests), #18 (no sanitizers), #21 (chunked decoding), #22 (TE validation), #23 (parse_body not defined), #24 (body_encoding dead field), #25 (Content-Encoding), #26 (Expect: 100-continue), #27 (error responses), #28 (MAX_BODY_LEN arbitrary), #29 (URI validation), #30 (pipelining), #31 (connection persistence), #32 (encode unimplemented) |
