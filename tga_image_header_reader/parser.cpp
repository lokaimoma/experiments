#include "parser.h"
#include <cstdint>
#include <fstream>
#include <optional>

std::optional<ErrorMsg> parse_header(Header &header, std::ifstream &file) {
  if (!file.is_open()) {
    return "File not open";
  }

  header.id_length = file.get();
  header.color_map_type = file.get();
  header.image_type = file.get();
  header.color_map_origin = fread_16<int16_t>(file);
  header.color_map_length = fread_16<int16_t>(file);
  header.color_map_depth = file.get();
  header.x_origin = fread_16<int16_t>(file);
  header.y_origin = fread_16<int16_t>(file);
  header.width = fread_16<uint16_t>(file);
  header.height = fread_16<uint16_t>(file);
  header.pixel_depth = file.get();
  header.image_descriptor = file.get();

  return std::nullopt;
}
