#pragma once
#include <functional>
#include <vector>

#include "header.hpp"

namespace http {
namespace server {

using addHeaderCallback = std::function<void(std::vector<Header> &headers)>;

}  // namespace server
}  // namespace http
