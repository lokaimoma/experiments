#include "parser.h"
#include <array>
#include <cstddef>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <zconf.h>
#include <zlib.h>

std::optional<ErrorMsg> PakFileMetaData::get_file_content(std::ifstream &file,
                                                          std::ofstream &out,
                                                          size_t index) const {
    if (index >= toc.size()) {
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

    std::vector<char> compressed_buf(t_entry.stored_size);
    if (!file.read(compressed_buf.data(), t_entry.stored_size)) {
        return stream_error_to_errormsg(file);
    }
    std::vector<char> uncompressed_buf{};

    switch (t_entry.method) {
    case StorageMethod::STORED:
        uncompressed_buf = std::move(compressed_buf);
        break;
    case StorageMethod::ZLIB: {
        uncompressed_buf.resize(t_entry.original_size);
        uLongf dest_len{t_entry.original_size};

        int result{
            uncompress(reinterpret_cast<Bytef *>(uncompressed_buf.data()),
                       &dest_len,
                       reinterpret_cast<Bytef *>(compressed_buf.data()),
                       t_entry.stored_size)};

        if (result != Z_OK) {
            std::cerr << zError(result) << '\n';
            return ErrorMsg::Z_LIB_DECOMPRESSION_ERROR;
        }
        break;
    }
    default:
        return ErrorMsg::UNKNOWN_STORAGE_METHOD;
    }

    if (!out.write(uncompressed_buf.data(), uncompressed_buf.size())) {
        return stream_error_to_errormsg(out);
    }

    return std::nullopt;
}

std::optional<ErrorMsg> parse_pak_file_metadata(PakFileMetaData &pak,
                                                std::ifstream &file) {
    if (!file.is_open()) {
        return ErrorMsg::INPUT_FILE_NOT_OPEN;
    }

    if (!file.read(reinterpret_cast<char *>(pak.header.magic.data()),
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
    pak.toc.clear();

    for (size_t i{0}; i < pak.header.file_count; ++i) {
        const int fn_len = file.get();
        if (fn_len == EOF) {
            return stream_error_to_errormsg(file);
        }

        TOCEntry toc_entry{};
        toc_entry.filename.resize(fn_len);
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
        toc_entry.stored_size = opt_fc.value();

        opt_fc = fread_uint32(file);
        if (!opt_fc) {
            return stream_error_to_errormsg(file);
        }
        toc_entry.original_size = opt_fc.value();

        const int storage_method = file.get();
        if (storage_method == EOF) {
            return stream_error_to_errormsg(file);
        }

        switch (static_cast<StorageMethod>(storage_method)) {
        case StorageMethod::STORED:
            toc_entry.method = StorageMethod::STORED;
            break;
        case StorageMethod::ZLIB:
            toc_entry.method = StorageMethod::ZLIB;
            break;
        default:
            std::cerr << "Uknown storage method: "
                      << std::to_string(storage_method) << '\n';
            return ErrorMsg::UNKNOWN_STORAGE_METHOD;
        }

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
