#pragma once

#include <cstdlib>
#include <string>

#include "beauty/parse_common.hpp"
#include "beauty/request.hpp"

namespace beauty {

class RequestDecoder {
   public:
    RequestDecoder() = default;
    virtual ~RequestDecoder() = default;

    bool decodeRequest(Request &req, std::vector<char> &content);

   private:
    void keyValDecode(const std::string &in,
                      std::vector<std::pair<std::string, std::string>> &params);

    template <typename InputIterator>
    void urlDecode(const InputIterator begin, const InputIterator end, std::string &escaped) {
        escaped.reserve(std::distance(begin, end));
        for (auto i = begin, nd = end; i < nd; ++i) {
            auto c = (*i);
            switch (c) {
                case '%':
                    // A percent-escape needs two more characters after '%', and
                    // both must be hex digits. Bounds-check before touching
                    // i[1]/i[2]: the input range may be a vector<char> with no
                    // terminating NUL, so reading past the end is a real
                    // out-of-bounds access. The previous code also passed a
                    // non-NUL-terminated 2-char array to strtol, which over-read
                    // adjacent bytes on every escape.
                    if (end - i > 2 && isHexDigit(static_cast<unsigned char>(i[1])) &&
                        isHexDigit(static_cast<unsigned char>(i[2]))) {
                        escaped += static_cast<char>(hexValue(static_cast<unsigned char>(i[1])) * 16 +
                                                     hexValue(static_cast<unsigned char>(i[2])));
                        i += 2;
                    } else {
                        // Malformed or truncated escape: keep the literal '%'.
                        escaped += '%';
                    }
                    break;
                case '+':
                    escaped += ' ';
                    break;
                default:
                    escaped += c;
            }
        }
    }
};

}  // namespace beauty
