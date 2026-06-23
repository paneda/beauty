#pragma once

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <vector>

#include "beauty/header.hpp"

namespace beauty {

// A response received from a server.
struct Response {
    friend class ResponseParser;

    Response(std::vector<char> &body) : body_(body) {}

    int httpVersionMajor_ = 0;
    int httpVersionMinor_ = 0;
    int statusCode_ = 0;
    std::string statusMessage_;
    std::vector<Header> headers_;
    bool keepAlive_ = true;
    std::vector<char> &body_;

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

    // returns number of body bytes received so far
    size_t getNoBodyBytesReceived() const {
        return body_.size();
    }

    // returns the content length declared by the server, or
    // std::numeric_limits<size_t>::max() if none was specified.
    size_t getContentLength() const {
        return contentLength_;
    }

    void reset() {
        httpVersionMajor_ = 0;
        httpVersionMinor_ = 0;
        statusCode_ = 0;
        statusMessage_.clear();
        headers_.clear();
        keepAlive_ = true;
        body_.clear();
        contentLength_ = std::numeric_limits<size_t>::max();
        isChunked_ = false;
    }

   private:
    static bool ichar_equals(char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    }

    bool iequals(const std::string &a, const std::string &b) const {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), ichar_equals);
    }

    size_t contentLength_ = std::numeric_limits<size_t>::max();  // means not specified
    bool isChunked_ = false;
};

}  // namespace beauty
