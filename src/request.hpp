#pragma once

#include <string>
#include <vector>

#include "header.hpp"

namespace http {
namespace server {

/// A request received from a client.
struct Request {
    std::string method_;
    std::string uri_;
    int http_version_major_;
    int http_version_minor_;
    std::vector<header> headers_;
};

}  // namespace server
}  // namespace http
