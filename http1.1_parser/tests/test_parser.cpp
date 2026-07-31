#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parser.h"

TEST_CASE("milestone 0 scaffold: skeleton compiles, dummy feed is a no-op") {
  Http11RequestParser parser;
  const uint8_t bytes[] = {'G', 'E', 'T'};
  CHECK(parser.feed(bytes, sizeof(bytes)) == ParseStatus::NeedMoreData);
  CHECK(parser.headers().empty());
  CHECK_FALSE(parser.last_error().has_value());
  parser.reset();
}
