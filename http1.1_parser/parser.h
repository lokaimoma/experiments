#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Limits {
  size_t max_request_line_len{8192};
  size_t max_header_count{100};
  size_t max_header_line_len{8192};
  size_t max_total_header_bytes{64 * 1024};
  size_t max_chunk_size_bytes{8 * 1024 * 1024};
  size_t max_body_bytes{10 * 1024 * 1024};
};

enum class ParserStage { StartLine, Headers, Body, Trailers, End };
enum class BodyType { None, Fixed, Chunked };
enum class ParseStatus { NeedMoreData, Complete, Error };
enum class ParseError {
  RequestLineTooLarge,
  InvalidRequestMethod,
  InvalidRequestTarget,
  InvalidHTTPVersion,
  InvalidStatusLine,

  InvalidHeaders,

  UnknownParserStage,
};

struct RequestLine {
  std::string method{};
  std::string request_target{};
  std::string http_version{};
};

struct HeaderField {
  std::string key{};
  std::string value{};
};

class Http11RequestParser {
private:
  RequestLine rl{};
  Limits limits{};
  std::vector<HeaderField> _headers{};
  std::vector<uint8_t> buf{};
  std::optional<ParseError> _last_error{};
  ParserStage stage{ParserStage::StartLine};
  BodyType body_type{BodyType::None};

  std::optional<size_t> parse_request_line();
  bool validate_req_method(std::string_view);
  bool validate_req_target(std::string_view);

  std::optional<size_t> parse_headers();

public:
  explicit Http11RequestParser(Limits l = Limits{}) : limits{l} {};

  ParseStatus feed(const uint8_t *data, size_t len);
  void reset();

  const RequestLine &request_line() const;
  const std::vector<HeaderField> &headers() const;
  std::optional<ParseError> last_error() const;
};
