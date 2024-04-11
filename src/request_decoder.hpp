#pragma once

#include <cstdlib>
#include <string>

#include "request.hpp"

namespace http {
namespace server {

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
                    if (i[1] && i[2]) {
                        char hs[]{i[1], i[2]};
                        escaped += static_cast<char>(std::strtol(hs, nullptr, 16));
                        i += 2;
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

}  // namespace server
}  // namespace http
