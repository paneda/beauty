#include "request_decoder.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

namespace http {
namespace server {

namespace {

bool ichar_equals(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

bool iequals(const std::string &a, const std::string &b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), ichar_equals);
}

}  // namespace

bool RequestDecoder::decodeRequest(Request &req) {
    // url decode uri
    urlDecode(req.uri_.begin(), req.uri_.end(), req.requestPath_);

    // request path must be absolute and not contain ".."
    if (req.requestPath_.empty() || req.requestPath_[0] != '/' ||
        req.requestPath_.find("..") != std::string::npos) {
        return false;
    }

    // decode the query string
    std::string queryStr;
    size_t pos = req.requestPath_.find('?');
    if (pos != std::string::npos) {
        keyValDecode(req.requestPath_.substr(pos + 1, std::string::npos), req.queryParams_);

        // remove the query string from path
        req.requestPath_ = req.requestPath_.substr(0, pos);
    }

    if (req.method_ != "GET") {
        // if header says so, decode form body
        auto it = std::find_if(req.headers_.begin(), req.headers_.end(), [](const Header &h) {
            return iequals(h.name_, "content-type") &&
                   iequals(h.value_, "application/x-www-form-urlencoded");
        });
        if (it != req.headers_.end()) {
            std::string bodyStr;
            urlDecode(req.body_.begin(), req.body_.end(), bodyStr);
            keyValDecode(bodyStr, req.formParams_);
        }
    }

    return true;
}

void RequestDecoder::keyValDecode(const std::string &in,
                                  std::vector<std::pair<std::string, std::string>> &params) {
    std::string key, val;
    auto out = &key;
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '=') {
            out = &val;
        } else if (in[i] == '&') {
            params.push_back({key, val});
            out = &key;
            key.clear();
            val.clear();
        } else if (i == in.size() - 1) {
            *out += in[i];
            params.push_back({key, val});
        } else {
            *out += in[i];
        }
    }
}

}  // namespace server
}  // namespace http
