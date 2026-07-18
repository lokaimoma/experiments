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

struct RawMessageHeader {
  std::vector<uint8_t> status_line{};
  std::vector<uint8_t> headers{};
};

struct ParsedMessageHeader {
  std::string method{};
  std::string path{};
  std::string version{};
  std::unordered_map<std::string, std::vector<std::string>> headers{};
};

struct HttpRequest {
  RequestParsingStage stage{RequestParsingStage::status_line};
  RawMessageHeader raw_headers{};
  ParsedMessageHeader parsed_headers{};
  size_t body_len{0};
  std::vector<uint8_t> body{};
};

struct HttpResponse {
  RawMessageHeader raw_headers{};
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
