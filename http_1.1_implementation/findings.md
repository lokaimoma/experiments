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

### 21. Chunked Transfer-Encoding not decoded
For `TE: chunked`, `body_len` is set to `MAX_BODY_LEN` and raw bytes are dumped verbatim into `conn.req.body`. Chunk-size hex lines, CRLFs, and trailing headers all become part of the body data. There is no dechunking loop, and `body_len` conflates "expected decoded size" with "max raw-read limit."

### 22. Transfer-Encoding: only strict `"chunked"` accepted, multi-value rejected
The code now rejects any non-`"chunked"` Transfer-Encoding with 501. This is a deliberate simplification — `TE: gzip, chunked` (valid per RFC 7230 §3.3.1) would be rejected despite being compliant. A future `parse_body` step would need to handle multi-value transfer codings.

---

## `http1_parser.h` — Parser Interface

### 23. `parse_body` declared but never defined
`static void parse_body(HttpConnection &)` is declared as a private static method (`http1_parser.h:20`) but has no definition. Dead declaration.

---

## `http_connection.h` — Data Structures


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
| **Bug** | 1 | #9 (server.run never called) |
| **Missing** | 14 | #16 (no graceful shutdown), #17 (no tests), #18 (no sanitizers), #21 (chunked decoding), #22 (TE validation), #23 (parse_body not defined), #25 (Content-Encoding), #26 (Expect: 100-continue), #27 (error responses), #28 (MAX_BODY_LEN arbitrary), #29 (URI validation), #30 (pipelining), #31 (connection persistence), #32 (encode unimplemented) |
