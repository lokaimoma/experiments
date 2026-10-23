#include "parser.h"
#include <array>
#include <cstddef>
#include <ios>
#include <optional>
#include <string>
#include <utility>

std::optional<ErrorMsg> PakFileMetaData::get_file_content(std::ifstream &file,
                                                          std::ofstream &out,
                                                          size_t index) const {
    if (index > toc.size()) {
        return ErrorMsg::INDEX_OUT_OF_RANGE;
    }

    if (!file.is_open()) {
        return ErrorMsg::INPUT_FILE_NOT_OPEN;
    }

    if (!out.is_open()) {
        return ErrorMsg::OUTPUT_FILE_NOT_OPEN;
    }

    const TOCEntry &t_entry{toc[index]};

    if (!file.seekg(t_entry.offset, std::ios::beg)) {
        return ErrorMsg::FILE_TOO_SMALL_FOR_REQUESTED_FILE_POSITION;
    }

    constexpr size_t BUFFER_SIZE{4096};
    std::array<char, BUFFER_SIZE> buf{};
    size_t count{t_entry.size};

    while (count > 0) {
        size_t to_read{std::min(BUFFER_SIZE, count)};
        if (!file.read(buf.data(), to_read)) {
            return stream_error_to_errormsg(file);
        }
        std::streamsize read{file.gcount()};
        out.write(buf.data(), read);
        count -= read;
    }

    return std::nullopt;
}

std::optional<ErrorMsg> parse_pak_file_metadata(PakFileMetaData &pak,
                                                std::ifstream &file) {
    if (!file.is_open()) {
        return ErrorMsg::INPUT_FILE_NOT_OPEN;
    }

    if (file.read(reinterpret_cast<char *>(pak.header.magic.data()),
                  pak.header.magic.size())) {
        return stream_error_to_errormsg(file);
    }

    if (!pak.is_valid()) {
        return ErrorMsg::INVALID_PAK_FILE;
    }

    auto opt_fc{fread_uint32(file)};
    if (!opt_fc) {
        return stream_error_to_errormsg(file);
    }
    pak.header.file_count = opt_fc.value();

    for (size_t i{0}; i < pak.header.file_count; ++i) {
        int8_t fn_len = file.get();
        if (fn_len == -1) {
            return stream_error_to_errormsg(file);
        }

        TOCEntry toc_entry{};
        toc_entry.filename.resize((fn_len));
        if (!file.read(toc_entry.filename.data(), fn_len)) {
            return stream_error_to_errormsg(file);
        }

        opt_fc = fread_uint32(file);
        if (!opt_fc) {
            return stream_error_to_errormsg(file);
        }
        toc_entry.offset = opt_fc.value();

        opt_fc = fread_uint32(file);
        if (!opt_fc) {
            return stream_error_to_errormsg(file);
        }
        toc_entry.size = opt_fc.value();

        pak.toc.push_back(std::move(toc_entry));
    }

    return std::nullopt;
}

std::optional<uint32_t> fread_uint32(std::ifstream &file) {
    uint8_t bytes[4];

    if (!file.read(reinterpret_cast<char *>(bytes), 4)) {
        return std::nullopt;
    }

    return static_cast<uint32_t>(bytes[0]) |
           static_cast<uint32_t>(bytes[1] << 8) |
           static_cast<uint32_t>(bytes[2] << 16) |
           static_cast<uint32_t>(bytes[3] << 24);
}

ErrorMsg stream_error_to_errormsg(std::ios &s) {
    switch (s.rdstate()) {
    case std::ios_base::eofbit:
        return ErrorMsg::EOF_REACHED;
    case (std::ios_base::eofbit | std::ios_base::failbit):
        return ErrorMsg::INCOMPLETE_DATA_EOF_REACHED;
    case std::ios_base::failbit:
        return ErrorMsg::STREAM_LOGICAL_ERROR;
    case std::ios_base::badbit:
        return ErrorMsg::STREAM_FATAL_ERROR;
    default:
        return ErrorMsg::STREAM_UNKNOWN_ERROR;
    }
}
