#include "parser.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view USAGE =
    "Usage:\n"
    "  cpkg_reader_mtester <archive.pak> list\n"
    "  cpkg_reader_mtester <archive.pak> info\n"
    "  cpkg_reader_mtester <archive.pak> extract <index> [output-path]\n";

std::string_view to_string(ErrorMsg error) {
  switch (error) {
  case ErrorMsg::INDEX_OUT_OF_RANGE:
    return "Index is out of range";
  case ErrorMsg::INPUT_FILE_NOT_OPEN:
    return "Input file is not open";
  case ErrorMsg::OUTPUT_FILE_NOT_OPEN:
    return "Output file is not open";
  case ErrorMsg::FILE_TOO_SMALL_FOR_REQUESTED_FILE_POSITION:
    return "File is too small for the requested position";
  case ErrorMsg::EOF_REACHED:
    return "End of file reached";
  case ErrorMsg::INCOMPLETE_DATA_EOF_REACHED:
    return "Unexpected end of file (incomplete data)";
  case ErrorMsg::STREAM_LOGICAL_ERROR:
    return "Stream logical error";
  case ErrorMsg::STREAM_FATAL_ERROR:
    return "Stream fatal error";
  case ErrorMsg::STREAM_UNKNOWN_ERROR:
    return "Unknown stream error";
  case ErrorMsg::INVALID_PAK_FILE:
    return "Invalid PAK file (bad magic signature)";
  case ErrorMsg::Z_LIB_DECOMPRESSION_ERROR:
    return "zlib decompression failed";
  case ErrorMsg::UNKNOWN_STORAGE_METHOD:
    return "Unknown storage method";
  }

  return "Unknown error";
}

std::string_view storage_method_name(StorageMethod method) {
  switch (method) {
  case StorageMethod::STORED:
    return "stored";
  case StorageMethod::ZLIB:
    return "zlib";
  }

  return "unknown";
}

std::optional<std::size_t> parse_index(std::string_view text) {
  std::size_t index{};
  const char *begin = text.data();
  const char *end = begin + text.size();

  const auto [ptr, ec] = std::from_chars(begin, end, index);
  if (ec != std::errc{} || ptr != end) {
    return std::nullopt;
  }

  return index;
}

void print_info(const PakFileMetaData &pak,
                const std::filesystem::path &archive_path) {
  std::uint64_t stored_size{0};
  std::uint64_t original_size{0};
  for (const auto &entry : pak.toc) {
    stored_size += entry.stored_size;
    original_size += entry.original_size;
  }

  std::error_code ec;
  const auto archive_size = std::filesystem::file_size(archive_path, ec);

  std::cout << "\n------PAK INFO------\n";
  std::cout << "Archive      : " << archive_path.string() << "\n";
  std::cout << "Magic        : "
            << std::string(pak.header.magic.begin(), pak.header.magic.end())
            << "\n";
  std::cout << "File count   : " << pak.header.file_count << "\n";
  if (ec) {
    std::cout << "Archive size : unknown\n";
  } else {
    std::cout << "Archive size : " << archive_size << " bytes\n";
  }
  std::cout << "Original size: " << original_size << " bytes\n";
  std::cout << "Stored size  : " << stored_size << " bytes\n";
}

void print_files(const PakFileMetaData &pak) {
  std::cout << "\n"
            << std::right << std::setw(5) << "INDEX" << "  " << std::setw(10)
            << "SIZE" << "  " << std::setw(10) << "STORED" << "  "
            << std::left << std::setw(8) << "METHOD" << std::right << "  "
            << std::setw(10) << "OFFSET" << "  NAME\n";

  for (std::size_t i{0}; i < pak.toc.size(); ++i) {
    const auto &entry = pak.toc[i];
    std::cout << std::right << std::setw(5) << i << "  " << std::setw(10)
              << entry.original_size << "  " << std::setw(10)
              << entry.stored_size << "  " << std::left << std::setw(8)
              << storage_method_name(entry.method) << std::right << "  "
              << std::setw(10) << entry.offset << "  " << entry.filename
              << "\n";
  }
}

int extract_file(const PakFileMetaData &pak, std::ifstream &archive,
                 std::size_t index, const std::filesystem::path &output_path) {
  std::ofstream out{output_path, std::ios::binary | std::ios::trunc};
  if (!out.is_open()) {
    std::cerr << "Could not open output file: " << output_path.string() << "\n";
    return EXIT_FAILURE;
  }

  const auto error = pak.get_file_content(archive, out, index);
  if (error) {
    std::cerr << "Extraction failed: " << to_string(*error) << "\n";
    return EXIT_FAILURE;
  }

  const auto &entry = pak.toc[index];
  std::cout << "Extracted '" << entry.filename << "' ("
            << entry.original_size << " bytes, " << entry.stored_size
            << " stored, " << storage_method_name(entry.method) << ") to '"
            << output_path.string() << "'\n";
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << USAGE;
    return EXIT_FAILURE;
  }

  const std::filesystem::path archive_path{argv[1]};
  const std::string_view command{argv[2]};

  std::ifstream archive{archive_path, std::ios::binary};
  if (!archive.is_open()) {
    std::cerr << "Could not open archive: " << archive_path.string() << "\n";
    return EXIT_FAILURE;
  }

  PakFileMetaData pak;
  const auto parse_error = parse_pak_file_metadata(pak, archive);
  if (parse_error) {
    std::cerr << "Failed to parse '" << archive_path.string()
              << "': " << to_string(*parse_error) << "\n";
    return EXIT_FAILURE;
  }

  if (command == "info") {
    print_info(pak, archive_path);
    return EXIT_SUCCESS;
  }

  if (command == "list") {
    print_files(pak);
    return EXIT_SUCCESS;
  }

  if (command == "extract") {
    if (argc < 4) {
      std::cerr << "extract requires a file index\n\n" << USAGE;
      return EXIT_FAILURE;
    }

    const auto index = parse_index(argv[3]);
    if (!index) {
      std::cerr << "Invalid file index: " << argv[3] << "\n";
      return EXIT_FAILURE;
    }

    if (*index >= pak.toc.size()) {
      std::cerr << "Index " << *index << " is out of range (archive has "
                << pak.toc.size() << " files)\n";
      return EXIT_FAILURE;
    }

    const std::filesystem::path output_path =
        argc >= 5 ? std::filesystem::path{argv[4]}
                  : std::filesystem::path{pak.toc[*index].filename};

    if (output_path.empty()) {
      std::cerr << "Output path is empty\n";
      return EXIT_FAILURE;
    }

    return extract_file(pak, archive, *index, output_path);
  }

  std::cerr << "Unknown command: " << command << "\n\n" << USAGE;
  return EXIT_FAILURE;
}
