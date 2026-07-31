#pragma once

#include "http_connection.h"
#include <cstddef>
#include <cstring>
#include <sys/types.h>

enum class ParseResult { Complete, NeedMoreData, Error };

class Http1Parser {
private:
  static void parse_status_line(HttpConnection &);

public:
  static ParseResult decode(HttpConnection &);
  static bool encode(HttpConnection &);
};
