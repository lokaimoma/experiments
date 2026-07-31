#include "parser.h"

ParseStatus Http11RequestParser::feed(const uint8_t *data, size_t len) {
  (void)data;
  (void)len;
  return ParseStatus::NeedMoreData;
}

void Http11RequestParser::reset() {}

const RequestLine &Http11RequestParser::request_line() const { return rl; }

const std::vector<HeaderField> &Http11RequestParser::headers() const {
  return _headers;
}

std::optional<ParseError> Http11RequestParser::last_error() const {
  return _last_error;
}
