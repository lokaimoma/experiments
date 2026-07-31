#pragma once

#include "unique_fd.h"
#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>

enum class RequestParsingStage {
  REQUEST_LINE,
  HEADERS,
  BODY_CHUNKED,
  BODY_CONTENT,
  BODY_TRAILER,
  COMPLETE,
  ERROR,
};

constexpr int MAX_HEADER_SIZE = 8192;             // 8KiB
constexpr int MAX_BODY_LEN = MAX_HEADER_SIZE * 2; // 16KiB

struct RequestParserContext {
  std::array<uint8_t, MAX_HEADER_SIZE> header_buf{};

  std::string_view method{};
  std::string_view path{};
  std::string_view version{};

  RequestParsingStage stage{RequestParsingStage::REQUEST_LINE};
};

struct ResponseContext {};

struct HttpConnection {
  RequestParserContext req{};
  ResponseContext response{};
  struct sockaddr_storage addr;
  UniqueFd fd{};
  bool want_read{false};
  bool want_write{false};
  bool want_close{false};
};
