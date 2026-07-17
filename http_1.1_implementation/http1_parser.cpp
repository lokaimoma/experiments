#include "http1_parser.h"
#include "http_connection.h"
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <unistd.h>

ParseResult Http1Parser::decode(HttpConnection *conn) {
  using enum ParseResult;
  uint8_t buff[64 * 1024];
  if (!conn) {
    return Error;
  }

  ssize_t size{read(conn->fd, buff, sizeof(buff))};

  if (size < 0) {
    if (errno == EAGAIN) {
      return NeedMoreData;
    }
    throw std::runtime_error("read: " + std::string(strerror(errno)));
  }

  if (size == 0) {
    return Complete;
  }

  switch (conn->req.stage) {
  case RequestParsingStage::status_line:
    Http1Parser::parse_status_line(conn, buff, size);
    break;
  case RequestParsingStage::headers:
    Http1Parser::parse_headers(conn, buff, size);
    break;
  case RequestParsingStage::body:
    Http1Parser::parse_body(conn, buff, size);
    break;
  case RequestParsingStage::end:
    return Complete;
  }

  return NeedMoreData;
}

void Http1Parser::parse_status_line(HttpConnection *conn, uint8_t *buf,
                                    ssize_t buf_len) {
  if (!buf || buf_len <= 0 ||
      conn->req.stage != RequestParsingStage::status_line) {
    return;
  }

  uint8_t *found{static_cast<uint8_t *>(std::memchr(buf, '\n', buf_len))};

  auto &status_line_buf{conn->req.header.status_line};

  if (found) {
    conn->req.stage = RequestParsingStage::headers;
    std::ptrdiff_t found_len{found - buf};

    size_t needed{status_line_buf.size() + found_len};
    if (status_line_buf.capacity() < needed) {
      status_line_buf.reserve(needed);
    }
    status_line_buf.insert(status_line_buf.end(), buf, found + 1);

    if (found + 1 < buf + buf_len) {
      conn->req.header.headers.insert(conn->req.header.headers.end(), found + 1,
                                      buf + buf_len);
    }
    return;
  }

  size_t needed{status_line_buf.size() + buf_len};
  if (status_line_buf.capacity() < needed) {
    status_line_buf.reserve(needed);
  }

  status_line_buf.insert(status_line_buf.end(), buf, buf + buf_len);
}
