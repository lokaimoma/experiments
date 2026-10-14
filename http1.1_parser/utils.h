#pragma once

#include <algorithm>
#include <string_view>

bool case_insensitive_equal(std::string_view s1, std::string_view s2) {
  return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(),
                    [](unsigned char a, unsigned char b) {
                      return std::tolower(a) == std::tolower(b);
                    });
}
