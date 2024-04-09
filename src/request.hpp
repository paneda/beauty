#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "header.hpp"

namespace http {
namespace server {

// A request received from a client.
struct Request {
    friend class Connection;
    friend class RequestParser;
    friend class RequestHandler;

    Request(std::vector<char> &body) : body_(body) {}

    std::string method_;
    std::string uri_;
    int httpVersionMajor_ = 0;
    int httpVersionMinor_ = 0;
    std::vector<Header> headers_;
    bool keepAlive_ = false;
    std::string requestPath_;
    std::vector<char> &body_;
    size_t bodySize_;

    // Parsed query params in the request
    std::vector<std::pair<std::string, std::string>> queryParams_;

    // Parsed form params in the request
    std::vector<std::pair<std::string, std::string>> formParams_;

    // convenience functions
    // case insensitive
    std::string getHeaderValue(const std::string &name) const {
        auto it = std::find_if(headers_.begin(), headers_.end(), [&](const Header &h) {
            return iequals(h.name_, name);
        });
        if (it != headers_.end()) {
            return it->value_;
        }
        return "";
    }

    struct Param {
        bool exist_;
        std::string value_;
    };

    // Note: below are case sensitive for speed
    Param getQueryParam(const std::string &key) const {
        return getParam(queryParams_, key);
    }

    Param getFormParam(const std::string &key) const {
        return getParam(formParams_, key);
    }

    // check if requestPath_ starts with specified string
    bool startsWith(const std::string &sw) const {
        return requestPath_.rfind(sw, 0) == 0;
    }

    // returns content-length value
    int getBodySize() const {
        return bodySize_;
    }

    // returns number of body bytes in initial request buffer
    int getNoInitialBodyBytesReceived() const {
        return noInitialBodyBytesReceived_;
    }

   private:
    void reset() {
        method_.clear();
        uri_.clear();
        headers_.clear();
        requestPath_.clear();
        body_.clear();
        bodySize_ = 0;
    }
    Param getParam(const std::vector<std::pair<std::string, std::string>> &params,
                   const std::string &key) const {
        auto it = std::find_if(
            params.begin(), params.end(), [&](const std::pair<std::string, std::string> &param) {
                return param.first == key;
            });
        if (it != params.end()) {
            return {true, it->second};
        }
        return {false, ""};
    }

    static bool ichar_equals(char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    }

    bool iequals(const std::string &a, const std::string &b) const {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), ichar_equals);
    }

    int noInitialBodyBytesReceived_ = -1;
};

}  // namespace server
}  // namespace http
