#include "parser.h"
#include "utils.h"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

ParseStatus Http11RequestParser::feed(const uint8_t *data, size_t len) {
  if (_last_error) {
    return ParseStatus::Error;
  }

  buf.insert(buf.end(), data, data + len);

  while (true) {
    std::optional<size_t> section_end{std::nullopt};

    switch (stage) {
    case ParserStage::StartLine:
      section_end = parse_request_line();
      break;

    case ParserStage::Headers:
      break;

    case ParserStage::Body:
      break;

    case ParserStage::Trailers:
      break;

    case ParserStage::End:
      return ParseStatus::Complete;

    default:
      _last_error = ParseError::UnknownParserStage;
      return ParseStatus::Error;
    }

    if (!section_end) {
      break;
    }

    buf.erase(buf.begin(),
              buf.begin() + std::min(section_end.value(), buf.size()));
    switch (stage) {
    case ParserStage::StartLine:
      stage = ParserStage::Headers;
      break;
    case ParserStage::Headers:
      stage = ParserStage::Body;
      break;
    case ParserStage::Body:
      stage = ParserStage::Trailers;
      break;
    case ParserStage::Trailers:
      stage = ParserStage::End;
      break;
    case ParserStage::End:
      return ParseStatus::Complete;
    default:
      _last_error = ParseError::UnknownParserStage;
      return ParseStatus::Error;
    }
  }

  if (_last_error) {
    return ParseStatus::Error;
  }

  return ParseStatus::NeedMoreData;
}

std::optional<size_t> Http11RequestParser::parse_request_line() {
  std::string_view buf_view{reinterpret_cast<const char *>(buf.data()),
                            buf.size()};

  std::string_view end_marker{"\r\n"};
  size_t idx{buf_view.find(end_marker)};

  if (idx == std::string_view::npos) {
    return std::nullopt;
  }

  buf_view = {buf_view.substr(0, idx)};

  if (buf_view.size() > limits.max_request_line_len) {
    _last_error = ParseError::RequestLineTooLarge;
    return std::nullopt;
  }

  size_t split_pos{0};

  for (auto sub_range : buf_view | std::views::split(' ')) {
    std::string_view req_line_comp{sub_range.data(), sub_range.size()};

    switch (split_pos) {
    case 0:
      if (!validate_req_method(req_line_comp)) {
        _last_error = ParseError::InvalidRequestMethod;
        return std::nullopt;
      }

      rl.method = to_lower_case(req_line_comp);
      break;

    case 1:
      if (!validate_req_target(req_line_comp)) {
        _last_error = ParseError::InvalidRequestTarget;
        return std::nullopt;
      }

      rl.request_target = std::string(req_line_comp);
      break;

    case 2:
      if (!case_insensitive_equal(req_line_comp, "HTTP/1.1")) {
        _last_error = ParseError::InvalidHTTPVersion;
        return std::nullopt;
      }

      rl.http_version = to_upper_case(req_line_comp);
      break;

    default:
      _last_error = ParseError::InvalidStatusLine;
      return std::nullopt;
      break;
    }

    ++split_pos;
  }

  if (split_pos < 3) {
    _last_error = ParseError::InvalidStatusLine;
    return std::nullopt;
  }

  return idx + end_marker.size();
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
