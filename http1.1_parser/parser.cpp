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
    std::string_view stl_comp{sub_range.data(), sub_range.size()};

    switch (split_pos) {
    case 0:
      if (!val_status_line_mthd(stl_comp)) {
        _last_error = ParseError::InvalidRequestMethod;
        return false;
      };
      rl.method = std::string(stl_comp);
      break;
    case 1:
      // validate here
      rl.request_target = std::string(stl_comp);
      break;
    case 2:
      // validate here
      rl.http_version = std::string(stl_comp);
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

bool Http11RequestParser::val_status_line_mthd(std::string_view s) {
  return case_insensitive_equal(s, "head") ||
         case_insensitive_equal(s, "get") ||
         case_insensitive_equal(s, "post") ||
         case_insensitive_equal(s, "put") ||
         case_insensitive_equal(s, "delete") ||
         case_insensitive_equal(s, "options") ||
         case_insensitive_equal(s, "connect") ||
         case_insensitive_equal(s, "trace");
}

const RequestLine &Http11RequestParser::request_line() const { return rl; }

const std::vector<HeaderField> &Http11RequestParser::headers() const {
  return _headers;
}

std::optional<ParseError> Http11RequestParser::last_error() const {
  return _last_error;
}
