#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Limits {
  size_t max_start_line_len{8192};
  size_t max_header_count{100};
  size_t max_header_line_len{8192};
  size_t max_total_header_bytes{64 * 1024};
  size_t max_chunk_size_bytes{8 * 1024 * 1024};
  size_t max_body_bytes{10 * 1024 * 1024};
};

enum class ParseStatus { NeedMoreData, Complete, Error };
enum class ParseError {};

struct RequestLine {
  std::string method{};
  std::string path{};
  std::string url{};
};

struct HeaderField {
  std::string key{};
  std::string value{};
};

class Http11RequestParser {
private:
  Limits limits{};
  RequestLine rl{};
  std::vector<HeaderField> _headers{};
  std::optional<ParseError> _last_error{};

public:
  explicit Http11RequestParser(Limits l = Limits{}) : limits{l} {};

  ParseStatus feed(const uint8_t *data, size_t len);
  void reset();

  const RequestLine &request_line() const;
  const std::vector<HeaderField> &headers() const;
  std::optional<ParseError> last_error() const;
};
