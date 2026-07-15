#include "server.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

void Server::close() {
  if (getaddrinfo_result) {
    freeaddrinfo(getaddrinfo_result);
  }
  if (sockfd != -1) {
    ::close(sockfd);
  }
}

Server::~Server() { close(); }

Server::Server(Server &&s) noexcept
    : sockfd{s.sockfd}, getaddrinfo_result{s.getaddrinfo_result},
      curraddrinfo{s.curraddrinfo} {
  s.close();
}

Server &Server::operator=(Server &&s) noexcept {
  if (this != &s) {
    close();
    sockfd = s.sockfd;
    getaddrinfo_result = s.getaddrinfo_result;
    curraddrinfo = s.curraddrinfo;
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

  if (curraddrinfo->ai_family == AF_INET) {
    raw_addr = &((struct sockaddr_in *)curraddrinfo->ai_addr)->sin_addr;
  } else {
    raw_addr = &((struct sockaddr_in6 *)curraddrinfo->ai_addr)->sin6_addr;
  }

  if (inet_ntop(curraddrinfo->ai_family, raw_addr, addr_str.data(),
                INET6_ADDRSTRLEN)) {
    std::cout << "listening on " << addr_str << '\n';
  } else {
    perror("inet_ntop");
  }

  while (true) {
    struct sockaddr_storage connaddr;
    socklen_t connaddrlen{sizeof(struct sockaddr_storage)};
    int connfd = accept(sockfd, (struct sockaddr *)&connaddr, &connaddrlen);
    if (connfd == -1) {
      perror("accept");
      continue;
    }

    struct timeval timeout{.tv_sec = 1, .tv_usec = 0};
    res =
        setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (res == -1) {
      perror("setsockopt(connfd)(SOL_SOCKET -> SO_RCVTIMEO)");
    }

    // impl read write logic

    ::close(connfd);
  }
}
