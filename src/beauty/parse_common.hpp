#pragma once

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstdint>

namespace beauty {

// Check if a byte is an HTTP character.
inline bool isChar(int c) {
    return c >= 0 && c <= 127;
}

// Check if a byte is an HTTP control character.
inline bool isCtl(int c) {
    return (c >= 0 && c <= 31) || (c == 127);
}

// Check if a byte is defined as an HTTP tspecial character.
inline bool isTsspecial(int c) {
    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
    }
}

// Check if a byte is a digit.
inline bool isDigit(int c) {
    return c >= '0' && c <= '9';
}

// Safely parse a non-negative decimal integer from a C string.
// Returns true and writes the value to *out on success.
// Returns false (and leaves *out unchanged) when the string is empty, contains
// non-digit characters, or the value overflows the target type.
// Unlike atoi this never invokes undefined behaviour on overflow and never
// silently returns 0 for garbage input.
inline bool parseUint(const char *str, size_t &out) {
    if (str == nullptr || *str == '\0') {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    unsigned long long v = strtoull(str, &end, 10);
    if (errno == ERANGE) {
        return false;
    }
    if (end == nullptr || *end != '\0' || end == str) {
        return false;
    }
    // Ensure the value fits in size_t (relevant when unsigned long long is
    // wider, though typically they are the same width).
    if (v > static_cast<unsigned long long>(SIZE_MAX)) {
        return false;
    }
    out = static_cast<size_t>(v);
    return true;
}

inline bool parseUint16(const char *str, uint16_t &out) {
    size_t v = 0;
    if (!parseUint(str, v)) {
        return false;
    }
    if (v > UINT16_MAX) {
        return false;
    }
    out = static_cast<uint16_t>(v);
    return true;
}

}  // namespace beauty
