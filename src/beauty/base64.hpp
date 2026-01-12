#pragma once

#include <string>
#include <stddef.h>

namespace beauty {

std::string base64_encode(const unsigned char* src, size_t len);
std::string base64_decode(const void* data, const size_t len);

// Zero-allocation version: encodes directly to provided buffer
// Returns number of bytes written (not including null terminator)
// Buffer must be at least ((len + 2) / 3) * 4 bytes + 1 for null terminator
size_t base64_encode_to_buffer(const unsigned char* src, size_t len, char* out, size_t out_size);

// Zero-allocation version: decodes directly to provided buffer
// Returns number of bytes written, or 0 on error
// Buffer must be at least (len / 4) * 3 bytes (decoded data is always smaller than encoded)
size_t base64_decode_to_buffer(const void* data, size_t len, unsigned char* out, size_t out_size);

}  // namespace beauty
