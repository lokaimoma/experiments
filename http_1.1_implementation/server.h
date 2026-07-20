#pragma once
#include <memory>
#include <netdb.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <utility>

class Server {
private:
  using ConnfdAddrPair = std::pair<int, struct sockaddr_storage>;

  int sockfd{-1};
  std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>
      getaddrinfo_result{nullptr, &::freeaddrinfo};

  void close() noexcept;
  void try_bind();
  void set_socket_options();
  std::optional<ConnfdAddrPair> handle_accept();

public:
  ~Server();

  explicit Server(std::string port = "0");

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  Server(Server &&) noexcept;
  Server &operator=(Server &&) noexcept;

  void listen();
  void run();
};
