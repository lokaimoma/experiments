#include "server.h"
#include "http_connection.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <optional>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

void fd_set_nonblock(int fd) {
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void Server::close() noexcept {
  if (getaddrinfo_result) {
    freeaddrinfo(getaddrinfo_result);
    getaddrinfo_result = nullptr;
  }
  if (sockfd != -1) {
    ::close(sockfd);
    sockfd = -1;
  }
}

Server::~Server() { close(); }

Server::Server(Server &&s) noexcept
    : sockfd{s.sockfd}, getaddrinfo_result{s.getaddrinfo_result} {
  s.close();
}

Server &Server::operator=(Server &&s) noexcept {
  if (this != &s) {
    close();
    sockfd = s.sockfd;
    getaddrinfo_result = s.getaddrinfo_result;
    s.close();
  }
  return *this;
}

void Server::set_socket_options() {
  int opt{1};
  int res{setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))};

  if (res == -1) {
    perror("set_socket_options(SOL_SOCKET -> SO_REUSEADDR = 1)");
  }

  int nodelay{1};
  res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
  if (res == -1) {
    perror("set_socket_options(IPPROTO_TCP -> TCP_NODELAY = 1)");
  }
}

void Server::try_bind() {

  struct addrinfo *curraddrinfo{nullptr};
  for (curraddrinfo = getaddrinfo_result; curraddrinfo != nullptr;
       curraddrinfo = curraddrinfo->ai_next) {
    sockfd = socket(curraddrinfo->ai_family, curraddrinfo->ai_socktype,
                    curraddrinfo->ai_protocol);
    if (sockfd == -1) {
      perror("try_bind:socket");
      continue;
    }

    set_socket_options();

    int res{::bind(sockfd, curraddrinfo->ai_addr, curraddrinfo->ai_addrlen)};
    if (res == -1) {
      perror("try_bind:bind");
      ::close(sockfd);
      sockfd = -1;
      continue;
    }
    break;
  }

  if (sockfd == -1) {
    throw std::runtime_error("try_bind: failed to create or bind socket");
  }
}

Server::Server(std::string port) {
  struct addrinfo addr_hints{};
  addr_hints.ai_family = AF_UNSPEC;
  addr_hints.ai_socktype = SOCK_STREAM;
  addr_hints.ai_protocol = IPPROTO_TCP;
  addr_hints.ai_flags = AI_PASSIVE;

  int res{getaddrinfo(nullptr, port.data(), &addr_hints, &getaddrinfo_result)};
  if (res != 0) {
    throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(res)));
  }

  try_bind();
}

void Server::listen() {
  int res{::listen(sockfd, SOMAXCONN)};
  if (res == -1) {
    throw std::runtime_error("listen:" + std::string(strerror(errno)));
  }

  std::string addr_str(INET6_ADDRSTRLEN, '\0');
  void *raw_addr{nullptr};

  struct sockaddr_storage addr{};
  socklen_t len{sizeof(addr)};
  getsockname(sockfd, (struct sockaddr *)&addr, &len);
  if (addr.ss_family == AF_INET) {
    raw_addr = &((struct sockaddr_in *)&addr)->sin_addr;
  } else {
    raw_addr = &((struct sockaddr_in6 *)&addr)->sin6_addr;
  }

  if (inet_ntop(addr.ss_family, raw_addr, addr_str.data(), INET6_ADDRSTRLEN)) {
    std::cout << "listening on " << addr_str << '\n';
  } else {
    perror("inet_ntop");
  }

  fd_set_nonblock(sockfd);
}

auto Server::handle_accept() -> std::optional<ConnfdAddrPair> {
  struct sockaddr_storage connaddr;
  socklen_t connaddrlen{sizeof(struct sockaddr_storage)};
  int connfd = accept(sockfd, (struct sockaddr *)&connaddr, &connaddrlen);

  if (connfd == -1) {
    switch (errno) {
    case EWOULDBLOCK:
    case ECONNABORTED:
    case EPROTO:
    case ENETDOWN:
    case ENOPROTOOPT:
    case EHOSTDOWN:
    case ENOENT:
    case EHOSTUNREACH:
    case EOPNOTSUPP:
    case ENETUNREACH:
      return std::nullopt;
    default:
      throw std::runtime_error("handle_accept: " +
                               std::string(strerror(errno)));
    }
  }

  // struct timeval timeout{.tv_sec = 1, .tv_usec = 0};
  // int res{
  //     setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
  //     sizeof(timeout))};
  // if (res == -1) {
  //   perror("setsockopt(connfd)(SOL_SOCKET -> SO_RCVTIMEO)");
  // }
  fd_set_nonblock(connfd);

  return ConnfdAddrPair{connfd, connaddr};
}

void Server::run() {
  std::vector<std::unique_ptr<HttpConnection>> connections{};
  std::vector<struct pollfd> pollfds{};
  size_t closed_connections{0};

  while (true) {
    pollfds.clear();
    pollfds.reserve(connections.size() + 1);
    pollfds.emplace_back(sockfd, POLLIN, 0);

    for (auto &conn : connections) {
      if (!conn) {
        continue;
      }

      struct pollfd pfd{conn->fd, 0, 0};
      if (conn->want_read)
        pfd.events |= POLLIN;
      if (conn->want_write)
        pfd.events |= POLLOUT;
      pollfds.push_back(pfd);
    }

    int res = poll(pollfds.data(), pollfds.size(), -1);
    if (res == -1 && errno != EINTR) {
      throw std::runtime_error("poll failed: " + std::string(strerror(errno)));
    }

    if (pollfds[0].revents) {
      if (auto connfd_addr_pair_ = handle_accept()) {
        auto connfd_addr_pair{connfd_addr_pair_.value()};
        if (connections.capacity() <= connfd_addr_pair.first) {
          connections.resize(connfd_addr_pair.first + 1);
        }
        connections[connfd_addr_pair.first] = std::make_unique<HttpConnection>(
            HttpConnection{.addr = connfd_addr_pair.second,
                           .fd = connfd_addr_pair.first,
                           .want_read = true});
      }
    }

    for (size_t i = 1; i < pollfds.size(); ++i) {
      auto &pfd = pollfds[i];
      if (pfd.revents == 0)
        continue;

      auto &conn = connections[pfd.fd];
      if (!conn)
        continue;

      if (pfd.revents & POLLIN) {
        // handle_read(*conn);
      }

      if (pfd.revents & POLLOUT) {
        // handle_write(*conn);
      }

      if ((pfd.revents & (POLLHUP | POLLERR)) || conn->want_close) {
        ::close(conn->fd);
        conn.reset(nullptr);
        closed_connections++;
        continue;
      }
    }

    if (closed_connections > 1000) {
      connections.erase(
          std::remove(connections.begin(), connections.end(), nullptr),
          connections.end());
      closed_connections = 0;
    }
  }
}
