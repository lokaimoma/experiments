#pragma once

#include <string>
#include <string_view>

void tolowercase(std::string &s);
void touppercase(std::string &s);
template <typename T> T trim(T s) {
  auto first{s.find_first_not_of(" \t\n\r")};
  if (first == std::string_view::npos)
    return std::string{};
  auto last = s.find_last_not_of(" \t\n\r");
  return s.substr(first, last - first + 1);
}
