#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>

struct HttpRequest {
  std::string_view method{};
  std::string_view path{};
  std::vector<uint8_t> incoming{};
  std::unordered_map<std::string_view, std::string_view> headers{};
};

struct HttpResponse {
  std::vector<uint8_t> body{};
  std::unordered_map<std::string, std::string> headers{};
  int status;
};

struct HttpConnection {
  HttpRequest req{};
  HttpResponse resp{};
  struct sockaddr_storage addr;
  int fd{-1};
  bool want_read{false};
  bool want_write{false};
  bool want_close{false};
};
