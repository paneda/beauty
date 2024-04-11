#include <algorithm>
#include <cctype>
#include <sstream>

#include "request_decoder.hpp"

namespace http {
namespace server {

bool RequestDecoder::decodeRequest(Request &req, std::vector<char> &content) {
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
        if (req.getHeaderValue("content-type") == "application/x-www-form-urlencoded") {
            std::string bodyStr;
            urlDecode(content.begin(), content.end(), bodyStr);
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
