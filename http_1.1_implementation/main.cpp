#include "server.h"
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  try {
    std::string port = (argc > 1) ? argv[1] : "8080";
    Server server(port);
    server.listen();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
