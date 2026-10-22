// AI-GENERATED
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parser.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

class TempBinaryFile {
public:
  explicit TempBinaryFile(std::vector<std::uint8_t> bytes) {
    static std::atomic<unsigned> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("tga_header_reader_" + std::to_string(stamp) + "_" +
             std::to_string(counter++) + ".tga");

    std::ofstream out{path_, std::ios::binary | std::ios::trunc};
    REQUIRE(out.is_open());
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  ~TempBinaryFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  TempBinaryFile(const TempBinaryFile &) = delete;
  TempBinaryFile &operator=(const TempBinaryFile &) = delete;

  [[nodiscard]] std::ifstream open() const {
    return std::ifstream{path_, std::ios::binary};
  }

private:
  std::filesystem::path path_;
};

void append_u16(std::vector<std::uint8_t> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

std::vector<std::uint8_t> sample_header_bytes() {
  std::vector<std::uint8_t> bytes;
  bytes.push_back(4);
  bytes.push_back(1);
  bytes.push_back(2);
  append_u16(bytes, 256);
  append_u16(bytes, 100);
  bytes.push_back(24);
  append_u16(bytes, static_cast<std::uint16_t>(-3));
  append_u16(bytes, static_cast<std::uint16_t>(-4));
  append_u16(bytes, 640);
  append_u16(bytes, 426);
  bytes.push_back(32);
  bytes.push_back(0x28);
  return bytes;
}

} // namespace

TEST_CASE("fread_16 decodes little-endian uint16_t values") {
  TempBinaryFile file{{0x34, 0x12}};
  auto in = file.open();
  REQUIRE(in.is_open());

  CHECK(fread_16<std::uint16_t>(in) == 0x1234);
}

TEST_CASE("fread_16 decodes little-endian int16_t values") {
  SUBCASE("zero") {
    TempBinaryFile file{{0x00, 0x00}};
    auto in = file.open();
    REQUIRE(in.is_open());

    CHECK(fread_16<std::int16_t>(in) == 0);
  }

  SUBCASE("negative one") {
    TempBinaryFile file{{0xFF, 0xFF}};
    auto in = file.open();
    REQUIRE(in.is_open());

    CHECK(fread_16<std::int16_t>(in) == -1);
  }

  SUBCASE("minimum value") {
    TempBinaryFile file{{0x00, 0x80}};
    auto in = file.open();
    REQUIRE(in.is_open());

    CHECK(fread_16<std::int16_t>(in) == -32768);
  }
}

TEST_CASE("fread_16 consumes exactly two bytes") {
  TempBinaryFile file{{0x34, 0x12, 0xAB}};
  auto in = file.open();
  REQUIRE(in.is_open());

  CHECK(fread_16<std::uint16_t>(in) == 0x1234);
  CHECK(in.get() == 0xAB);
}

TEST_CASE("parse_header rejects a file that is not open") {
  std::ifstream file;
  REQUIRE_FALSE(file.is_open());

  Header header{};
  const auto error = parse_header(header, file);

  REQUIRE(error.has_value());
  CHECK(*error == "File not open");
}

TEST_CASE("parse_header reads every field of an 18-byte TGA header") {
  TempBinaryFile file{sample_header_bytes()};
  auto in = file.open();
  REQUIRE(in.is_open());

  Header header{};
  const auto error = parse_header(header, in);

  REQUIRE_FALSE(error.has_value());
  CHECK(header.id_length == 4);
  CHECK(header.color_map_type == 1);
  CHECK(header.image_type == 2);
  CHECK(header.color_map_origin == 256);
  CHECK(header.color_map_length == 100);
  CHECK(header.color_map_depth == 24);
  CHECK(header.x_origin == -3);
  CHECK(header.y_origin == -4);
  CHECK(header.width == 640);
  CHECK(header.height == 426);
  CHECK(header.pixel_depth == 32);
  CHECK(header.image_descriptor == 0x28);
}

TEST_CASE("parse_header leaves bytes after the header unread") {
  auto bytes = sample_header_bytes();
  bytes.push_back(0xAA);

  TempBinaryFile file{std::move(bytes)};
  auto in = file.open();
  REQUIRE(in.is_open());

  Header header{};
  REQUIRE_FALSE(parse_header(header, in).has_value());

  CHECK(in.get() == 0xAA);
}
