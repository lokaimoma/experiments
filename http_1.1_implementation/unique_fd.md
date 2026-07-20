# UniqueFd — RAII File Descriptor Wrapper

## Design

```cpp
struct UniqueFd {
    int fd{-1};

    ~UniqueFd() { if (fd != -1) ::close(fd); }

    UniqueFd() = default;
    explicit UniqueFd(int f) : fd{f} {}

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& o) noexcept : fd{std::exchange(o.fd, -1)} {}
    UniqueFd& operator=(UniqueFd&& o) noexcept {
        if (this != &o) {
            if (fd != -1) ::close(fd);
            fd = std::exchange(o.fd, -1);
        }
        return *this;
    }

    void reset(int f = -1) {
        if (fd != -1) ::close(fd);
        fd = f;
    }

    int release() { return std::exchange(fd, -1); }
};
```

## Usage

### `http_connection.h`
```cpp
struct HttpConnection {
    // ...
    UniqueFd fd{};
    // ...
};
```

When the `HttpConnection` is destroyed (e.g., erasing from the map), `UniqueFd::~UniqueFd()` closes the fd automatically.

### `server.h`
```cpp
class Server {
    // ...
    UniqueFd sockfd{};
    // ...
};
```

No need for `close()` method — the destructor runs automatically.

### `server.cpp` changes

**Move ops** — just move the `UniqueFd`:
```cpp
Server::Server(Server &&s) noexcept
    : sockfd{std::move(s.sockfd)},
      getaddrinfo_result{std::move(s.getaddrinfo_result)} {}

Server &Server::operator=(Server &&s) noexcept {
    if (this != &s) {
        sockfd = std::move(s.sockfd);
        getaddrinfo_result = std::move(s.getaddrinfo_result);
    }
    return *this;
}
```

**Destructor** — remove `close()` call entirely; member destructors handle cleanup.

**`set_socket_options`** — take fd as parameter:
```cpp
void Server::set_socket_options(int fd) { /* use fd instead of sockfd */ }
```

**`try_bind`** — wrap each socket attempt:
```cpp
UniqueFd tmp{socket(...)};
if (tmp.fd == -1) continue;  // socket() failed
set_socket_options(tmp.fd);
// ... bind(tmp.fd, ...) ...
// On success: sockfd = std::move(tmp);
```

**Accept handler** — wrap immediately:
```cpp
UniqueFd connfd{accept(sockfd.fd, ...)};
// ... setup ...
return ConnfdAddrPair{connfd.release(), connaddr};
```

**Event loop** — no more `::close(conn->fd)`:
```cpp
if ((pfd.revents & (POLLHUP | POLLERR)) || conn->want_close) {
    fd_to_conn.erase(conn->fd);  // UniqueFd closes automatically
    continue;
}
```

### `http1_parser.cpp`

`conn.fd` works as-is since `.fd` is a public `int` member of `UniqueFd`.
