#pragma once
#include <string>

class Server {
private:
  int sockfd{-1};
  struct addrinfo *getaddrinfo_result{nullptr};
  struct addrinfo *curraddrinfo{nullptr};

  void close();
  void try_bind();
  void set_socket_options();

public:
  ~Server();

  explicit Server(std::string port = "0");

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  Server(Server &&) noexcept;
  Server &operator=(Server &&) noexcept;

  void listen();
};
