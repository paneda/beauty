#pragma once

#include <string>

namespace http {
namespace server {

struct Header {
    std::string name_;
    std::string value_;
};

}  // namespace server
}  // namespace http
