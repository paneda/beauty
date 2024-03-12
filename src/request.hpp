#pragma once

#include <string>
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
};

}  // namespace server
}  // namespace http
