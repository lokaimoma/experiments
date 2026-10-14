#pragma once

#include <string>
#include <string_view>

bool case_insensitive_equal(std::string_view s1, std::string_view s2);
std::string to_lower_case(std::string_view s);
