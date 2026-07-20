#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>

enum class RequestParsingStage {
  status_line,
  headers,
  body,
  end,
};

struct HttpRequest {
  RequestParsingStage stage{RequestParsingStage::status_line};
  std::vector<uint8_t> raw_status_line{};
  std::vector<uint8_t> raw_headers{};
  std::string method{};
  std::string path{};
  std::string version{};
  std::unordered_map<std::string, std::vector<std::string>> headers{};
  size_t body_len{0};
  std::vector<uint8_t> body{};
};

struct HttpResponse {
  std::vector<uint8_t> raw_status_line{};
  std::vector<uint8_t> raw_headers{};
  std::vector<uint8_t> body{};
};

struct HttpConnection {
  HttpRequest req{};
  HttpResponse response{};
  struct sockaddr_storage addr;
  int fd{-1};
  bool want_read{false};
  bool want_write{false};
  bool want_close{false};
};
