#include "parser.h"
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
    res = parse_status_line();
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

bool Http11RequestParser::parse_status_line() {
  std::string_view sv{reinterpret_cast<const char *>(buf.data()), buf.size()};
  size_t idx{sv.find("\r\n")};
  if (idx == std::string_view::npos) {
    return false;
  }
  sv = {sv.substr(0, idx)};

  auto split_range{sv | std::views::split(' ')};
  auto it{split_range.begin()};
  auto end{split_range.end()};

  if (it == end) {
    _last_error = ParseError::RequestMethodNotFound;
    return false;
  }
  auto sub_range{*it};
  std::string_view stl_comp{sub_range.data(), sub_range.size()};
  // We check if it's equal to a valid verb here, etc
  rl.method = std::string(stl_comp);

  ++it;
  if (it == end) {
    _last_error = ParseError::NoRequestTargetFound;
    return false;
  }
  sub_range = *it;
  stl_comp = {sub_range.data(), sub_range.size()};
  // We check if it's valid origin or absolute request target, etc
  rl.request_target = std::string(stl_comp);

  ++it;
  if (it == end) {
    _last_error = ParseError::NoHttpVersionFound;
    return false;
  }
  sub_range = *it;
  stl_comp = {sub_range.data(), sub_range.size()};
  // We check if it's valid http version, etc
  rl.http_version = std::string(stl_comp);

  return true;
}

const RequestLine &Http11RequestParser::request_line() const { return rl; }

const std::vector<HeaderField> &Http11RequestParser::headers() const {
  return _headers;
}

std::optional<ParseError> Http11RequestParser::last_error() const {
  return _last_error;
}
