#pragma once

#include "http_connection.h"
#include <cstddef>
#include <cstring>
#include <sys/types.h>

enum class ParseResult { Complete, NeedMoreData, Error };

class Http1Parser {
private:
  static void parse_status_line(HttpConnection *, uint8_t *buff,
                                ssize_t buff_len);
  static void parse_headers(HttpConnection *, uint8_t *buff, ssize_t buff_len);
  static void parse_body(HttpConnection *, uint8_t *buff, ssize_t buff_len);

public:
  static constexpr int MAX_HEADER_SIZE = 8000;             // 8KB
  static constexpr int MAX_BODY_LEN = MAX_HEADER_SIZE * 2; // 16KB

  static ParseResult decode(HttpConnection *);
  static bool encode(HttpConnection *);
};
