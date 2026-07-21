#include "server.h"
#include "http_connection.h"
#include "unique_fd.h"
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
#include <unordered_map>
#include <utility>
#include <vector>

void fd_set_nonblock(int fd) {
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void Server::set_socket_options() {
  int opt{1};
  int res{setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))};

  if (res == -1) {
    perror("set_socket_options(SOL_SOCKET -> SO_REUSEADDR = 1)");
  }
}

void Server::try_bind() {

  struct addrinfo *curraddrinfo{nullptr};
  for (curraddrinfo = getaddrinfo_result.get(); curraddrinfo != nullptr;
       curraddrinfo = curraddrinfo->ai_next) {
    sockfd.reset(socket(curraddrinfo->ai_family, curraddrinfo->ai_socktype,
                        curraddrinfo->ai_protocol));
    if (sockfd == -1) {
      perror("try_bind:socket");
      continue;
    }

    set_socket_options();

    int res{::bind(sockfd, curraddrinfo->ai_addr, curraddrinfo->ai_addrlen)};
    if (res == -1) {
      perror("try_bind:bind");
      sockfd.reset();
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

  struct addrinfo *addr_info{nullptr};
  int res{getaddrinfo(nullptr, port.data(), &addr_hints, &addr_info)};
  if (res != 0) {
    throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(res)));
  }
  getaddrinfo_result.reset(addr_info);

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
  UniqueFd connfd{accept(sockfd, (struct sockaddr *)&connaddr, &connaddrlen)};

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

  int nodelay{1};
  int res{
      setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay))};
  if (res == -1) {
    perror("set_socket_options(IPPROTO_TCP -> TCP_NODELAY = 1)");
  }

  return ConnfdAddrPair{std::move(connfd), connaddr};
}

void Server::run() {
  std::unordered_map<int, std::unique_ptr<HttpConnection>> fd_to_conn{};
  std::vector<struct pollfd> pollfds{};

  while (true) {
    pollfds.clear();
    pollfds.reserve(fd_to_conn.size() + 1);

    pollfds.emplace_back(sockfd, POLLIN, 0);

    for (auto &fd_conn_pair : fd_to_conn) {
      auto &fd{fd_conn_pair.first};
      auto &conn{fd_conn_pair.second};

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

    if (pollfds[LISTEN_SOCKET_INDEX].revents) {
      if (auto connfd_addr_pair_ = handle_accept()) {
        auto connfd_addr_pair{std::move(connfd_addr_pair_.value())};
        int fd{connfd_addr_pair.first};

        fd_to_conn.insert({fd, std::make_unique<HttpConnection>(HttpConnection{
                                   .addr = connfd_addr_pair.second,
                                   .fd = std::move(connfd_addr_pair.first),
                                   .want_read = true})});
      }
    }

    for (size_t i = LISTEN_SOCKET_INDEX + 1; i < pollfds.size(); ++i) {
      auto &pfd = pollfds[i];
      if (pfd.revents == 0)
        continue;

      if (!fd_to_conn.contains(pfd.fd))
        continue;

      auto &connfd_addr_pair = fd_to_conn[pfd.fd];
      auto &conn{connfd_addr_pair};
      if (!conn) {
        fd_to_conn.erase(pfd.fd);
        continue;
      }

      if (pfd.revents & POLLIN) {
        // handle_read(*conn);
      }

      if (pfd.revents & POLLOUT) {
        // handle_write(*conn);
      }

      if ((pfd.revents & (POLLHUP | POLLERR)) || conn->want_close) {
        fd_to_conn.erase(conn->fd);
        continue;
      }
    }
  }
}
