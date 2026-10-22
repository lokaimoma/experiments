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

struct PakFileMetaData {
  PakHeader header{};
  std::vector<TOCEntry> toc{};

  bool is_valid() {
    return std::equal(header.magic.begin(), header.magic.end(),
                      PAK_MAGIC_SIGNATURE.begin(), PAK_MAGIC_SIGNATURE.end());
  }
};

using ErrorMsg = std::string; // better to use an enum

std::optional<ErrorMsg> parse_pak_file_metadata(PakFileMetaData &,
                                                std::ifstream &);

std::optional<uint32_t> fread_uint32(std::ifstream &file);
