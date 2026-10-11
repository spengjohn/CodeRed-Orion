#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace base64 {

std::string encode(const std::uint8_t* data, std::size_t len);
std::string encode(const std::vector<std::uint8_t>& bytes);

}
