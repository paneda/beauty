#pragma once
#include <functional>
#include <vector>

#include "header.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

using handlerCallback = std::function<void(const Request &req, Reply &rep)>;

}  // namespace server
}  // namespace http
