#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <vector>

constexpr std::array<uint8_t, 4> PAK_MAGIC_SIGNATURE{0x50, 0x41, 0x4B, 0x31};

enum class ErrorMsg {
    INDEX_OUT_OF_RANGE,
    INPUT_FILE_NOT_OPEN,
    OUTPUT_FILE_NOT_OPEN,
    FILE_TOO_SMALL_FOR_REQUESTED_FILE_POSITION,
    EOF_REACHED,
    INCOMPLETE_DATA_EOF_REACHED,
    STREAM_LOGICAL_ERROR,
    STREAM_FATAL_ERROR,
    STREAM_UNKNOWN_ERROR,
    INVALID_PAK_FILE,
};

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

    bool is_valid() const {
        return std::equal(header.magic.begin(),
                          header.magic.end(),
                          PAK_MAGIC_SIGNATURE.begin(),
                          PAK_MAGIC_SIGNATURE.end());
    }
    std::optional<ErrorMsg> get_file_content(std::ifstream &file,
                                             std::ofstream &out,
                                             size_t index) const;
};

std::optional<ErrorMsg> parse_pak_file_metadata(PakFileMetaData &,
                                                std::ifstream &);

std::optional<uint32_t> fread_uint32(std::ifstream &file);

ErrorMsg stream_error_to_errormsg(std::ios &s);
