#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

constexpr std::array<uint8_t, 4> PAK_MAGIC_SIGNATURE{0x50, 0x41, 0x4B, 0x31};

struct PakHeader {
  std::array<uint8_t, 4> magic{};
  uint32_t file_count{};
};

struct TOCEntry { // Entry in Table of Content
  uint32_t offset{};
  uint32_t size{};
  std::string filename{};
};

struct PakFile {
  PakHeader header{};
  std::vector<TOCEntry> toc{};
  std::vector<uint8_t> data{};

  bool is_valid() {
    return std::equal(header.magic.begin(), header.magic.end(),
                      PAK_MAGIC_SIGNATURE.begin(), PAK_MAGIC_SIGNATURE.end());
  }
};

