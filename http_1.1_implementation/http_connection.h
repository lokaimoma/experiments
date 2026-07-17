#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <vector>

enum class RequestParsingStage {
  status_line,
  headers,
  body,
  end,
};

struct MessageHeader {
  std::vector<uint8_t> status_line{};
  std::vector<uint8_t> headers{};
};

struct HttpMessage {
  RequestParsingStage stage{RequestParsingStage::status_line};
  MessageHeader header{};
  size_t body_len{0};
  std::vector<uint8_t> body{};
};

struct HttpConnection {
  HttpMessage req{};
  HttpMessage resp{};
  struct sockaddr_storage addr;
  int fd{-1};
  bool want_read{false};
  bool want_write{false};
  bool want_close{false};
};
