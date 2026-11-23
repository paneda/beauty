#pragma once

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <string>

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

// Check if a byte is a hexadecimal digit.
inline bool isHexDigit(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Convert a hexadecimal digit to its numeric value. Returns 0 for non-hex input
// (callers are expected to validate with isHexDigit first).
inline int hexValue(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;
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
    // strtoull itself skips leading whitespace and accepts a '+' or '-' sign
    // (negating the result), so require a pure digit sequence up front.
    for (const char *p = str; *p != '\0'; ++p) {
        if (!isDigit(static_cast<unsigned char>(*p))) {
            return false;
        }
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

// Parse the value of a Content-Length header: 1*DIGIT, optionally surrounded by
// whitespace (RFC 7230 3.2 / 3.3.2). Returns false for anything else, including
// signed values and values that overflow size_t.
inline bool parseContentLength(const std::string &value, size_t &out) {
    const char *ows = " \t";
    const size_t first = value.find_first_not_of(ows);
    if (first == std::string::npos) {
        return false;
    }
    const size_t last = value.find_last_not_of(ows);
    size_t v = 0;
    if (!parseUint(value.substr(first, last - first + 1).c_str(), v)) {
        return false;
    }
    out = v;
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
