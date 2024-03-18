#pragma once

#include <istream>
#include <string>
#include <vector>

#include "reply.hpp"

namespace {
const std::string StartJson = "{\"Error:\" \"";
const std::string EndJson = "\"}";
}

struct HttpResult2 {
    HttpResult2(std::vector<char>& body) : body_(body) {}
    void setError(int statusCode, const std::string& errMsg) {
        statusCode_ = static_cast<http::server::Reply::status_type>(statusCode);
        body_.reserve(StartJson.size() + errMsg.size() + EndJson.size());
        *this << StartJson << errMsg << EndJson;
    }

    HttpResult2& operator<<(const std::string& val) {
        body_.insert(body_.end(), val.begin(), val.end());
        return *this;
    }

    http::server::Reply::status_type statusCode_ = http::server::Reply::status_type::ok;
    std::vector<char>& body_;
    std::string contentLength() {
        return std::to_string(body_.size());
    };
};