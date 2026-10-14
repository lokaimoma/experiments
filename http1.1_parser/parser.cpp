#include "parser.h"
#include "utils.h"
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>

ParseStatus Http11RequestParser::feed(const uint8_t *data, size_t len) {
  if (_last_error) {
    return ParseStatus::Error;
  }

  buf.insert(buf.end(), data, data + len);

  bool res{false};

  switch (stage) {
  case ParserStage::StartLine:
    res = parse_request_line();
    break;

  case ParserStage::Headers:
    break;

  case ParserStage::Body:
    break;

  case ParserStage::Trailers:
    break;

  case ParserStage::End:
    break;
  }

  if (res) {
    // transition to next stage or mark as complete
  }

  if (_last_error) {
    return ParseStatus::Error;
  }

  return ParseStatus::NeedMoreData;
}

bool Http11RequestParser::parse_request_line() {
  std::string_view buf_view{reinterpret_cast<const char *>(buf.data()),
                            buf.size()};

  size_t idx{buf_view.find("\r\n")};

  if (idx == std::string_view::npos) {
    return false;
  }

  buf_view = {buf_view.substr(0, idx)};

  size_t split_pos{0};

  for (auto sub_range : buf_view | std::views::split(' ')) {
    std::string_view req_line_comp{sub_range.data(), sub_range.size()};

    switch (split_pos) {
    case 0:
      if (!validate_req_method(req_line_comp)) {
        _last_error = ParseError::InvalidRequestMethod;
        return false;
      }

      rl.method = to_lower_case(req_line_comp);
      break;

    case 1:
      if (!validate_req_target(req_line_comp)) {
        _last_error = ParseError::InvalidRequestTarget;
        return false;
      }

      rl.request_target = std::string(req_line_comp);
      break;

    case 2:
      if (!case_insensitive_equal(req_line_comp, "HTTP/1.1")) {
        _last_error = ParseError::InvalidHTTPVersion;
        return false;
      }

      rl.http_version = to_upper_case(req_line_comp);
      break;

    default:
      _last_error = ParseError::InvalidStatusLine;
      return false;
      break;
    }

    ++split_pos;
  }

  if (split_pos < 3) {
    _last_error = ParseError::InvalidStatusLine;
    return false;
  }

  return true;
}

bool Http11RequestParser::validate_req_method(std::string_view s) {
  return case_insensitive_equal(s, "head") ||
         case_insensitive_equal(s, "get") ||
         case_insensitive_equal(s, "post") ||
         case_insensitive_equal(s, "put") ||
         case_insensitive_equal(s, "delete") ||
         case_insensitive_equal(s, "options") ||
         case_insensitive_equal(s, "connect") ||
         case_insensitive_equal(s, "trace");
}

bool Http11RequestParser::validate_req_target(std::string_view s) {
  if (s.empty()) {
    return false;
  }

  if (s == "*" && rl.method != "options") {
    return false;
  }

  if (rl.method == "connect") {
    if (s.starts_with("/")) {
      return false;
    }

    if (s.find(":") == std::string_view::npos) {
      return false;
    }
  }

  if (s.find("://") == std::string_view::npos || s[0] != '/') {
    return false;
  }

  return true;
}

const RequestLine &Http11RequestParser::request_line() const { return rl; }

const std::vector<HeaderField> &Http11RequestParser::headers() const {
  return _headers;
}

std::optional<ParseError> Http11RequestParser::last_error() const {
  return _last_error;
}
