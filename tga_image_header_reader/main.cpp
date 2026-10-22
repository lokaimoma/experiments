#include "parser.h"
#include <cstdlib>
#include <iostream>

void read_header(std::string_view file_name) {
  std::ifstream file{file_name.data(), std::ios::binary};
  Header h{};
  parse_header(h, file);

  std::cout << "\n------TGA HEADER INFO------\n";
  std::cout << "Image Type   : " << static_cast<int>(h.image_type) << "\n";
  std::cout << "Width        : " << h.width << " px\n";
  std::cout << "Height       : " << h.height << " px\n";
  std::cout << "Pixel Depth  : " << static_cast<int>(h.pixel_depth) << " bpp\n";
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "TGA file name required\n";
    std::cerr << "Usage: <program_name> <tga_file_path>\n";
    return EXIT_FAILURE;
  }
  read_header(argv[1]);
  return EXIT_SUCCESS;
}
