#include "parser.h"
#include <optional>
#include <utility>

std::optional<ErrorMsg> parse_pak_file(PakFileMetaData &pak,
                                       std::ifstream &file) {
  if (!file.is_open()) {
    return "File not open";
  }

  if (file.read(reinterpret_cast<char *>(pak.header.magic.data()),
                pak.header.magic.size())) {
    return "Error reading file"; // Logging or returning actual error here
  }

  if (!pak.is_valid()) {
    return "Not a valid .pak file";
  }

  auto opt_fc{fread_uint32(file)};
  if (!opt_fc) {
    return "Error reading file"; // Logging or returning actual error here
  }
  pak.header.file_count = opt_fc.value();

  for (size_t i{0}; i < pak.header.file_count; ++i) {
    int8_t fn_len = file.get();
    if (fn_len == -1) {
      return "Error reading file"; // Logging or returning actual error here
    }

    TOCEntry toc_entry{};
    toc_entry.filename.resize((fn_len));
    if (!file.read(toc_entry.filename.data(), fn_len)) {
      return "Error reading file"; // Logging or returning actual error here
    }

    opt_fc = fread_uint32(file);
    if (!opt_fc) {
      return "Error reading file"; // Logging or returning actual error here
    }
    toc_entry.offset = opt_fc.value();

    opt_fc = fread_uint32(file);
    if (!opt_fc) {
      return "Error reading file"; // Logging or returning actual error here
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
