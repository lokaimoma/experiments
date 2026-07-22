#pragma once

#include "unique_fd.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
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

struct RequestParserContext {
  size_t body_content_remaining{0};
  std::vector<uint8_t> status_line_buf{};
  std::vector<uint8_t> headers_buf{};
  std::vector<uint8_t> body_buf{};
  std::string method{};
  std::string path{};
  std::string version{};
  std::unordered_map<std::string, std::vector<std::string>> headers{};
  std::optional<std::string> body_encoding{};
  RequestParsingStage stage{RequestParsingStage::REQUEST_LINE};
  bool is_body_chunked{false};
  bool reading_chunk_size{false};
};

struct ResponseContext {
  std::vector<uint8_t> raw_status_line{};
  std::vector<uint8_t> raw_headers{};
  std::vector<uint8_t> body{};
};

struct HttpConnection {
  RequestParserContext req{};
  ResponseContext response{};
  struct sockaddr_storage addr;
  UniqueFd fd{};
  bool want_read{false};
  bool want_write{false};
  bool want_close{false};
};
