#pragma once
#include "unique_fd.h"
#include <memory>
#include <netdb.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <utility>

class Server {
private:
  constexpr static size_t LISTEN_SOCKET_INDEX = 0;

  using ConnfdAddrPair = std::pair<UniqueFd, struct sockaddr_storage>;

  UniqueFd sockfd{};
  std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>
      getaddrinfo_result{nullptr, &::freeaddrinfo};

  void try_bind();
  void set_socket_options();
  std::optional<ConnfdAddrPair> handle_accept();

public:
  explicit Server(std::string port = "0");

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  Server(Server &&) noexcept = default;
  Server &operator=(Server &&) noexcept = default;

  void listen();
  void run();
};
