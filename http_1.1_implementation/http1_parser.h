#pragma once

#include "http_connection.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <sys/types.h>

enum class ParseResult { Complete, NeedMoreData, Error };

class Http1Parser {
private:
  static void parse_status_line(HttpConnection *, std::span<const uint8_t> buf);
  static void parse_headers(HttpConnection *, std::span<const uint8_t> buf);
  static void parse_body(HttpConnection *, std::span<const uint8_t> buf);

public:
  static constexpr int MAX_HEADER_SIZE = 8 * 1024;                // 8KiB
  static constexpr int MAX_BODY_LEN = MAX_HEADER_SIZE * 2;        // 16KiB
  static constexpr size_t READ_BUFFER_SIZE = MAX_HEADER_SIZE * 8; // 64KiB

  static ParseResult decode(HttpConnection *);
  static bool encode(HttpConnection *);
};
