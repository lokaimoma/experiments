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
| **Missing** | 2 | #17 (no tests), #18 (no sanitizers) |
