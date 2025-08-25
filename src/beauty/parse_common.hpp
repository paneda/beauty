#pragma once

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

}  // namespace beauty
