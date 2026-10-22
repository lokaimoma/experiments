#include "parser.h"
#include <cstdint>
#include <fstream>
#include <iostream>
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

void read_header(std::string_view file_name) {
  std::ifstream file{file_name.data(), std::ios::binary};
  Header h{};
  parse_header(h, file);

  std::cout << "\n------TGA HEADER INFO------\n";
  std::cout << "Image Type   :" << h.image_type << "\n";
  std::cout << "Width        :" << h.width << " px\n";
  std::cout << "Height       :" << h.height << " px\n";
  std::cout << "Pixel Depth  :" << h.pixel_depth << " bpp\n";
}
