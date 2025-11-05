#pragma once

#include <string>
#include <stddef.h>

namespace beauty {

std::string base64_encode(const unsigned char* src, size_t len);
std::string base64_decode(const void* data, const size_t len);

}  // namespace beauty
