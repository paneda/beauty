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

    std::string method_;
    std::string uri_;
    int httpVersionMajor_ = 0;
    int httpVersionMinor_ = 0;
    std::vector<Header> headers_;
    std::vector<char> body_;
    bool keepAlive_ = false;
    std::string requestPath_;

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

    // Note: below are case sensitive for speed
    std::string getQueryParamValue(const std::string &key) const {
        return getParamValue(queryParams_, key);
    }
    std::string getFormParamValue(const std::string &key) const {
        return getParamValue(formParams_, key);
    }

   private:
    void reset() {
        method_.clear();
        uri_.clear();
        headers_.clear();
        body_.clear();
        requestPath_.clear();
    }

    std::string getParamValue(const std::vector<std::pair<std::string, std::string>> &params,
                              const std::string &key) const {
        auto it = std::find_if(
            params.begin(), params.end(), [&](const std::pair<std::string, std::string> &param) {
                return param.first == key;
            });
        if (it != params.end()) {
            return it->second;
        }
        return "";
    }

    static bool ichar_equals(char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    }

    bool iequals(const std::string &a, const std::string &b) const {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), ichar_equals);
    }
};

}  // namespace server
}  // namespace http
