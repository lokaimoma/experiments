#pragma once

#include <concepts>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

/**
  Actual byte order in file

  uint8_t id_length{};
  uint8_t color_map_type{};
  uint8_t image_type{}; // data type code
  int16_t color_map_origin;
  int16_t color_map_length;
  uint8_t color_map_depth;
  int16_t x_origin{};
  int16_t y_origin{};
  uint16_t width{};
  uint16_t height{};
  uint8_t pixel_depth{}; // bits per pixel
  uint8_t image_descriptor{};
**/
struct Header {
  int16_t color_map_origin;
  int16_t color_map_length;
  int16_t x_origin{};
  int16_t y_origin{};
  uint16_t width{};
  uint16_t height{};
  uint8_t id_length{};
  uint8_t color_map_type{};
  uint8_t image_type{}; // data type code
  uint8_t color_map_depth;
  uint8_t pixel_depth{}; // bits per pixel
  uint8_t image_descriptor{};
};

template <typename T>
  requires std::same_as<T, uint16_t> || std::same_as<T, int16_t>
T fread_16(std::ifstream &file) {
  uint8_t bytes[2];

  file.read(reinterpret_cast<char *>(&bytes), 2);

  uint16_t raw{static_cast<uint16_t>(bytes[0] | (bytes[1] << 8))};

  return static_cast<T>(raw);
}

using ErrorMsg = std::string;

std::optional<ErrorMsg> parse_header(Header &, std::ifstream &file);

void read_header(std::string_view file_name);
