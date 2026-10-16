#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

bool case_insensitive_equal(std::string_view s1, std::string_view s2) {
  return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(),
                    [](unsigned char a, unsigned char b) {
                      return std::tolower(a) == std::tolower(b);
                    });
}

std::string to_upper_case(std::string_view s) {
  std::string ss{s};
  std::transform(ss.begin(), ss.end(), ss.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return ss;
}

std::string to_lower_case(std::string_view s) {
  std::string ss{s};
  std::transform(ss.begin(), ss.end(), ss.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return ss;
}

void trim(std::string &s) {
  const std::string_view whitespace{" \t\n\r\f\v"};

  size_t end{s.find_last_not_of(whitespace)};

  if (end == std::string::npos) {
    s.clear();
    return;
  }

  s.erase(end + 1);

  size_t start{s.find_first_not_of(whitespace)};
  if (start != std::string::npos) {
    s.erase(0, start);
  }
}
