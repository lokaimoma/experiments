#include "http1_parser.h"
#include "http_connection.h"
#include "str_utils.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

ParseResult Http1Parser::decode(HttpConnection &conn) {
  using enum ParseResult;

  std::array<uint8_t, READ_BUFFER_SIZE> buf{};

  ssize_t len_read{read(conn.fd, buf.data(), buf.size())};

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

  switch (conn.req.stage) {
  case RequestParsingStage::status_line:
    Http1Parser::read_status_line(conn, buf_span);
    break;

  case RequestParsingStage::headers:
    Http1Parser::read_headers(conn, buf_span);
    break;

  case RequestParsingStage::body:
    Http1Parser::read_body(conn, buf_span);
    break;

  case RequestParsingStage::end:
    return Complete;
  }

  return NeedMoreData;
}

void Http1Parser::read_status_line(HttpConnection &conn,
                                   std::span<const uint8_t> buf) {
  if (buf.empty() || conn.req.stage != RequestParsingStage::status_line) {
    return;
  }

  auto lf_pos{std::find(buf.begin(), buf.end(), '\n')};
  auto &status_line_buf{conn.req.raw_status_line};

  if (lf_pos != buf.end()) {
    conn.req.stage = RequestParsingStage::headers;

    status_line_buf.insert(status_line_buf.end(), buf.begin(), lf_pos + 1);

    parse_status_line(conn);

    if (lf_pos + 1 != buf.end()) {
      conn.req.raw_headers.insert(conn.req.raw_headers.end(), lf_pos + 1,
                                  buf.end());
    }
    return;
  }

  status_line_buf.insert(status_line_buf.end(), buf.begin(), buf.end());
}

void Http1Parser::parse_status_line(HttpConnection &conn) {
  auto &status_line_buf{conn.req.raw_status_line};

  std::string_view status_line_view{
      reinterpret_cast<char *>(status_line_buf.data()), status_line_buf.size()};

  auto pipeline{
      status_line_view | std::views::split(' ') |

      std::views::transform([](auto &&subrange) {
        auto first = &*subrange.begin();
        auto size = static_cast<size_t>(std::ranges::distance(subrange));
        return trim(std::string_view(first, size));
      }) |

      std::views::filter([](std::string_view s) { return !s.empty(); }) |
      std::views::take(3)};

  std::vector<std::string_view> parts(3);
  std::copy_n(pipeline.begin(), parts.size(), parts.begin());

  if (parts.size() != 3) {
    throw std::runtime_error(
        "invalid status line.Send 400 Bad request instead of throw.");
  } else {
    auto &req{conn.req};

    req.method = std::string(parts[0]);
    touppercase(req.method);

    req.path = std::string(parts[1]);

    req.version = std::string(parts[2]);
    touppercase(req.version);

    std::vector<uint8_t> temp;
    temp.swap(status_line_buf);

    if (req.version != "HTTP/1.1") {
      // To-do: Replace with HTTP error response
      throw std::runtime_error(
          "unsupported http version.Send 400 Bad request instead of throw.");
    }
    // Other checks will be added as we progress
  }
}

void Http1Parser::read_headers(HttpConnection &conn,
                               std::span<const uint8_t> buf) {
  if (buf.empty() || conn.req.stage != RequestParsingStage::headers) {
    return;
  }

  auto &headers_buf{conn.req.raw_headers};

  if (headers_buf.size() + buf.size() > MAX_HEADER_SIZE) {
    throw std::runtime_error("header too large. To Be Updated to write the "
                             "error to the response and close the connection.");
  }

  constexpr std::array<uint8_t, 4> double_crlf_pattern{'\r', '\n', '\r', '\n'};

  auto double_crlf_pos{std::search(buf.begin(), buf.end(),
                                   double_crlf_pattern.begin(),
                                   double_crlf_pattern.end())};

  if (double_crlf_pos != buf.end()) {
    conn.req.stage = RequestParsingStage::body;

    headers_buf.insert(headers_buf.end(), buf.begin(),
                       double_crlf_pos + double_crlf_pattern.size());

    parse_headers(conn);

    if (conn.req.body_len == 0) {
      conn.req.body_len = 0;
      conn.req.stage = RequestParsingStage::end;
      return;
    }

    if (double_crlf_pos + double_crlf_pattern.size() != buf.end()) {
      conn.req.body.insert(conn.req.body.end(),
                           double_crlf_pos + double_crlf_pattern.size(),
                           buf.end());
    }

    return;
  }

  headers_buf.insert(headers_buf.end(), buf.begin(), buf.end());
}

void Http1Parser::parse_headers(HttpConnection &conn) {
  auto &req{conn.req};
  auto &headers_buf{req.raw_headers};
  auto &headers{req.headers};

  std::string_view headers_view{reinterpret_cast<char *>(headers_buf.data()),
                                headers_buf.size()};

  auto crlf_pos{headers_view.find("\r\n")};

  while (crlf_pos != std::string_view::npos) {
    auto line{trim(headers_view.substr(0, crlf_pos))};
    if (line.empty()) {
      break;
    }
    auto colon_pos{line.find(":")};

    if (colon_pos == std::string_view::npos) {
      // TODO: Replace throw with an HTTP error
      throw std::runtime_error(
          "Invalid header value: " + std::string(line) +
          ". An http error will be thrown instead of this.");
    }

    std::string k{trim(line.substr(0, colon_pos))};
    tolowercase(k);
    std::string v{trim(line.substr(colon_pos + 1))};

    if (!headers.contains(k)) {
      headers.insert({k, std::vector<std::string>{v}});
    } else {
      headers[k].push_back(v);
    }

    if (crlf_pos + 2 >= headers_view.size()) {
      break;
    }
    headers_view = {headers_view.substr(crlf_pos + 2)};
    crlf_pos = headers_view.find("\r\n");
  }

  std::vector<uint8_t> temp;
  temp.swap(headers_buf);

  auto &req_method{req.method};

  size_t &body_len{req.body_len};

  if (req_method == "POST" || req_method == "PATCH" || req_method == "PUT") {
    if (headers.contains("transfer-encoding")) {

      if (headers["transfer-encoding"][0].find("chunked") !=
          std::string::npos) {
        // set req.body_encoding here
        body_len = MAX_BODY_LEN;
      } else {
        // To-do: 501 not implemented
        throw std::runtime_error("501 not implemented");
      }

    } else if (headers.contains("content-length")) {
      auto &content_length_vec{headers["content-length"]};

      if (content_length_vec.size() > 1) {
        auto it{std::adjacent_find(content_length_vec.cbegin(),
                                   content_length_vec.cend(),
                                   [](auto &i, auto &b) { return i != b; })};
        if (it != content_length_vec.cend()) {
          // To-do: 400 bad request
          throw std::runtime_error("400 bad request. Invalid content length");
        }
      }

      auto &cn{content_length_vec[0]};
      auto [ptr, ec] =
          std::from_chars(cn.data(), cn.data() + cn.size(), body_len);

      if (ec != std::errc{}) {
        // To-do : 400 bad request
        throw std::runtime_error("400 bad request");
      }
    }

    if (body_len == 0) {
      // To-do: 411: Length required error response
      throw std::runtime_error("411: Length required error");
    }

    if (body_len > MAX_BODY_LEN) {
      // To-do: 413: Content Too Large
      throw std::runtime_error("413: Content Too Large");
    }
  }
}

void Http1Parser::read_body(HttpConnection &conn,
                            std::span<const uint8_t> buf) {
  if (buf.empty() || conn.req.stage != RequestParsingStage::body) {
    return;
  }

  auto &headers{conn.req.headers};
  auto &body_buf{conn.req.body};
  auto body_len{conn.req.body_len};

  if (body_buf.size() + buf.size() > body_len) {
    // To-do: 413: Content Too Large
    throw std::runtime_error("413: Content Too Large");
  }

  body_buf.insert(body_buf.end(), buf.begin(), buf.end());

  if (body_buf.size() >= body_len) {
    conn.req.stage = RequestParsingStage::end;
  }
}
