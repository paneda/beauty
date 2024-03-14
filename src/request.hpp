#pragma once

#include <string>
#include <utility>
#include <vector>

#include "header.hpp"

namespace http {
namespace server {

// A request received from a client.
struct Request {
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
};

}  // namespace server
}  // namespace http
