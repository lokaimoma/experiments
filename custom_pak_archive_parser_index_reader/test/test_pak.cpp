#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parser.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

class TempFile {
public:
  explicit TempFile(std::vector<std::uint8_t> bytes = {}) {
    static std::atomic<unsigned> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("cpkg_reader_" + std::to_string(stamp) + "_" +
             std::to_string(counter++) + ".pak");

    std::ofstream out{path_, std::ios::binary | std::ios::trunc};
    REQUIRE(out.is_open());
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;

  [[nodiscard]] std::ifstream open_input() const {
    return std::ifstream{path_, std::ios::binary};
  }

  [[nodiscard]] std::ofstream open_output() const {
    return std::ofstream{path_, std::ios::binary | std::ios::trunc};
  }

  [[nodiscard]] std::string read_all() const {
    std::ifstream in{path_, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{in},
                       std::istreambuf_iterator<char>{}};
  }

private:
  std::filesystem::path path_;
};

struct FileSpec {
  std::string name;
  std::string data;
};

void append_u32(std::vector<std::uint8_t> &bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

std::vector<std::uint8_t> make_pak(const std::vector<FileSpec> &files) {
  std::vector<std::uint8_t> bytes;
  bytes.insert(bytes.end(), PAK_MAGIC_SIGNATURE.begin(),
               PAK_MAGIC_SIGNATURE.end());
  append_u32(bytes, static_cast<std::uint32_t>(files.size()));

  std::uint32_t offset{8};
  for (const auto &file : files) {
    offset += 1 + static_cast<std::uint32_t>(file.name.size()) + 4 + 4;
  }

  for (const auto &file : files) {
    bytes.push_back(static_cast<std::uint8_t>(file.name.size()));
    bytes.insert(bytes.end(), file.name.begin(), file.name.end());
    append_u32(bytes, offset);
    append_u32(bytes, static_cast<std::uint32_t>(file.data.size()));
    offset += static_cast<std::uint32_t>(file.data.size());
  }

  for (const auto &file : files) {
    bytes.insert(bytes.end(), file.data.begin(), file.data.end());
  }

  return bytes;
}

} // namespace

TEST_CASE("fread_uint32 decodes zero") {
  TempFile file{{0x00, 0x00, 0x00, 0x00}};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  CHECK(fread_uint32(in).value_or(1) == 0);
}

TEST_CASE("fread_uint32 decodes a typical value") {
  TempFile file{{0x78, 0x56, 0x34, 0x12}};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  CHECK(fread_uint32(in).value_or(0) == 0x12345678);
}

TEST_CASE("fread_uint32 decodes a value with the high bit set") {
  TempFile file{{0xEF, 0xBE, 0xAD, 0xDE}};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  CHECK(fread_uint32(in).value_or(0) == 0xDEADBEEF);
}

TEST_CASE("fread_uint32 returns nullopt for truncated input") {
  TempFile file{{0x01, 0x02}};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  CHECK_FALSE(fread_uint32(in).has_value());
}

TEST_CASE("fread_uint32 consumes exactly four bytes") {
  TempFile file{{0xDD, 0xCC, 0xBB, 0xAA, 0x7F}};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  CHECK(fread_uint32(in).value_or(0) == 0xAABBCCDD);
  CHECK(in.get() == 0x7F);
}

TEST_CASE("stream_error_to_errormsg maps stream states") {
  SUBCASE("good state") {
    std::ifstream stream;
    CHECK(stream_error_to_errormsg(stream) == ErrorMsg::STREAM_UNKNOWN_ERROR);
  }

  SUBCASE("eof only") {
    std::ifstream stream;
    stream.setstate(std::ios::eofbit);
    CHECK(stream_error_to_errormsg(stream) == ErrorMsg::EOF_REACHED);
  }

  SUBCASE("eof and fail") {
    std::ifstream stream;
    stream.setstate(std::ios::eofbit | std::ios::failbit);
    CHECK(stream_error_to_errormsg(stream) ==
          ErrorMsg::INCOMPLETE_DATA_EOF_REACHED);
  }

  SUBCASE("fail only") {
    std::ifstream stream;
    stream.setstate(std::ios::failbit);
    CHECK(stream_error_to_errormsg(stream) == ErrorMsg::STREAM_LOGICAL_ERROR);
  }

  SUBCASE("bad") {
    std::ifstream stream;
    stream.setstate(std::ios::badbit);
    CHECK(stream_error_to_errormsg(stream) == ErrorMsg::STREAM_FATAL_ERROR);
  }
}

TEST_CASE("PakFileMetaData::is_valid validates the magic signature") {
  SUBCASE("default metadata is invalid") {
    PakFileMetaData pak;
    CHECK_FALSE(pak.is_valid());
  }

  SUBCASE("PAK1 magic is valid") {
    PakFileMetaData pak;
    pak.header.magic = PAK_MAGIC_SIGNATURE;
    CHECK(pak.is_valid());
  }

  SUBCASE("a different magic is invalid") {
    PakFileMetaData pak;
    pak.header.magic = {'P', 'A', 'K', '2'};
    CHECK_FALSE(pak.is_valid());
  }
}

TEST_CASE("parse_pak_file_metadata rejects a closed input") {
  std::ifstream in;
  REQUIRE_FALSE(in.is_open());

  PakFileMetaData pak;
  const auto error = parse_pak_file_metadata(pak, in);

  REQUIRE(error.has_value());
  CHECK(*error == ErrorMsg::INPUT_FILE_NOT_OPEN);
}

TEST_CASE("parse_pak_file_metadata rejects an invalid magic signature") {
  TempFile file{{'N', 'O', 'P', 'E', 0x00, 0x00, 0x00, 0x00}};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  PakFileMetaData pak;
  const auto error = parse_pak_file_metadata(pak, in);

  REQUIRE(error.has_value());
  CHECK(*error == ErrorMsg::INVALID_PAK_FILE);
}

TEST_CASE("parse_pak_file_metadata rejects a truncated header") {
  TempFile file{{'P', 'A', 'K'}};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  PakFileMetaData pak;
  const auto error = parse_pak_file_metadata(pak, in);

  REQUIRE(error.has_value());
  CHECK(*error == ErrorMsg::INCOMPLETE_DATA_EOF_REACHED);
}

TEST_CASE("parse_pak_file_metadata reads the header and table of contents") {
  const auto bytes = make_pak({{"a.txt", "abc"}, {"b.bin", "WXYZ"}});
  TempFile file{bytes};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  PakFileMetaData pak;
  const auto error = parse_pak_file_metadata(pak, in);

  REQUIRE_FALSE(error.has_value());
  CHECK(pak.header.magic == PAK_MAGIC_SIGNATURE);
  CHECK(pak.header.file_count == 2);
  REQUIRE(pak.toc.size() == 2);

  CHECK(pak.toc[0].filename == "a.txt");
  CHECK(pak.toc[0].offset == 36);
  CHECK(pak.toc[0].size == 3);

  CHECK(pak.toc[1].filename == "b.bin");
  CHECK(pak.toc[1].offset == 39);
  CHECK(pak.toc[1].size == 4);
}

TEST_CASE("parse_pak_file_metadata accepts an empty archive") {
  TempFile file{make_pak({})};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  PakFileMetaData pak;
  const auto error = parse_pak_file_metadata(pak, in);

  REQUIRE_FALSE(error.has_value());
  CHECK(pak.header.file_count == 0);
  CHECK(pak.toc.empty());
}

TEST_CASE("parse_pak_file_metadata reports a truncated table of contents") {
  std::vector<std::uint8_t> bytes{PAK_MAGIC_SIGNATURE.begin(),
                                  PAK_MAGIC_SIGNATURE.end()};
  append_u32(bytes, 1);

  TempFile file{bytes};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  PakFileMetaData pak;
  const auto error = parse_pak_file_metadata(pak, in);

  REQUIRE(error.has_value());
  CHECK(*error == ErrorMsg::INCOMPLETE_DATA_EOF_REACHED);
}

TEST_CASE("parse_pak_file_metadata supports filenames up to 255 bytes") {
  const std::string name(200, 'n');
  TempFile file{make_pak({{name, "x"}})};
  auto in = file.open_input();
  REQUIRE(in.is_open());

  PakFileMetaData pak;
  const auto error = parse_pak_file_metadata(pak, in);

  REQUIRE_FALSE(error.has_value());
  REQUIRE(pak.toc.size() == 1);
  CHECK(pak.toc[0].filename == name);
  CHECK(pak.toc[0].size == 1);
}

TEST_CASE("parse_pak_file_metadata replaces an existing table of contents") {
  TempFile file{make_pak({{"a.txt", "abc"}})};
  PakFileMetaData pak;

  {
    auto in = file.open_input();
    REQUIRE(in.is_open());
    REQUIRE_FALSE(parse_pak_file_metadata(pak, in).has_value());
  }

  {
    auto in = file.open_input();
    REQUIRE(in.is_open());
    REQUIRE_FALSE(parse_pak_file_metadata(pak, in).has_value());
  }

  REQUIRE(pak.toc.size() == 1);
  CHECK(pak.toc[0].filename == "a.txt");
}

TEST_CASE("get_file_content rejects an out-of-range index") {
  TempFile input_file{make_pak({{"only.txt", "data"}})};
  TempFile output_file;
  auto in = input_file.open_input();
  auto out = output_file.open_output();
  REQUIRE(in.is_open());
  REQUIRE(out.is_open());

  PakFileMetaData pak;
  REQUIRE_FALSE(parse_pak_file_metadata(pak, in).has_value());

  CHECK(pak.get_file_content(in, out, pak.toc.size()) ==
        ErrorMsg::INDEX_OUT_OF_RANGE);
  CHECK(pak.get_file_content(in, out, pak.toc.size() + 10) ==
        ErrorMsg::INDEX_OUT_OF_RANGE);
}

TEST_CASE("get_file_content requires open streams") {
  TempFile input_file{make_pak({{"only.txt", "data"}})};
  TempFile output_file;
  auto in = input_file.open_input();
  auto out = output_file.open_output();
  REQUIRE(in.is_open());
  REQUIRE(out.is_open());

  PakFileMetaData pak;
  REQUIRE_FALSE(parse_pak_file_metadata(pak, in).has_value());

  SUBCASE("input is closed") {
    std::ifstream closed_in;
    CHECK(pak.get_file_content(closed_in, out, 0) ==
          ErrorMsg::INPUT_FILE_NOT_OPEN);
  }

  SUBCASE("output is closed") {
    std::ofstream closed_out;
    CHECK(pak.get_file_content(in, closed_out, 0) ==
          ErrorMsg::OUTPUT_FILE_NOT_OPEN);
  }
}

TEST_CASE("get_file_content extracts an entry into the output stream") {
  TempFile input_file{make_pak({{"a.txt", "abc"}, {"b.bin", "WXYZ"}})};
  TempFile output_file;
  auto in = input_file.open_input();
  auto out = output_file.open_output();
  REQUIRE(in.is_open());
  REQUIRE(out.is_open());

  PakFileMetaData pak;
  REQUIRE_FALSE(parse_pak_file_metadata(pak, in).has_value());

  SUBCASE("first entry") {
    REQUIRE_FALSE(pak.get_file_content(in, out, 0).has_value());
    out.close();
    CHECK(output_file.read_all() == "abc");
  }

  SUBCASE("second entry") {
    REQUIRE_FALSE(pak.get_file_content(in, out, 1).has_value());
    out.close();
    CHECK(output_file.read_all() == "WXYZ");
  }
}

TEST_CASE("get_file_content copies blobs larger than the read buffer") {
  std::string blob;
  blob.reserve(5000);
  for (std::size_t i{0}; i < 5000; ++i) {
    blob.push_back(static_cast<char>('a' + (i % 26)));
  }

  TempFile input_file{make_pak({{"large.bin", blob}})};
  TempFile output_file;
  auto in = input_file.open_input();
  auto out = output_file.open_output();
  REQUIRE(in.is_open());
  REQUIRE(out.is_open());

  PakFileMetaData pak;
  REQUIRE_FALSE(parse_pak_file_metadata(pak, in).has_value());

  REQUIRE_FALSE(pak.get_file_content(in, out, 0).has_value());
  out.close();
  CHECK(output_file.read_all() == blob);
}
