#include "http1_parser.h"
#include "http_connection.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <unistd.h>

ParseResult Http1Parser::decode(HttpConnection *conn) {
  using enum ParseResult;
  std::array<uint8_t, READ_BUFFER_SIZE> buf{};
  if (!conn) {
    return Error;
  }

  ssize_t len_read{read(conn->fd, buf.data(), buf.size())};

  if (len_read < 0) {
    if (errno == EAGAIN) {
      return NeedMoreData;
    }
    throw std::runtime_error("read: " + std::string(strerror(errno)));
  }

  if (len_read == 0) {
    return Complete;
  }

  std::span<const uint8_t> buf_span(buf.data(), len_read);
  switch (conn->req.stage) {
  case RequestParsingStage::status_line:
    Http1Parser::parse_status_line(conn, buf_span);
    break;
  case RequestParsingStage::headers:
    Http1Parser::parse_headers(conn, buf_span);
    break;
  case RequestParsingStage::body:
    Http1Parser::parse_body(conn, buf_span);
    break;
  case RequestParsingStage::end:
    return Complete;
  }

  return NeedMoreData;
}

void Http1Parser::parse_status_line(HttpConnection *conn,
                                    std::span<const uint8_t> buf) {
  if (buf.empty() || conn->req.stage != RequestParsingStage::status_line) {
    return;
  }

  auto lf_pos{std::find(buf.begin(), buf.end(), '\n')};
  auto &status_line_buf{conn->req.header.status_line};

  if (lf_pos != buf.end()) {
    conn->req.stage = RequestParsingStage::headers;

    status_line_buf.insert(status_line_buf.end(), buf.begin(), lf_pos + 1);

    if (lf_pos + 1 != buf.end()) {
      conn->req.header.headers.insert(conn->req.header.headers.end(),
                                      lf_pos + 1, buf.end());
    }
    return;
  }

  status_line_buf.insert(status_line_buf.end(), buf.begin(), buf.end());
}

// void Http1Parser::parse_headers(HttpConnection *conn,
//                                 std::span<const uint8_t> buff) {
//   if (!buf || buf_len <= 0 ||
//       conn->req.stage != RequestParsingStage::status_line) {
//     return;
//   }

//   constexpr uint8_t target[] = {'\r', '\n', '\r', '\n'};
// }
