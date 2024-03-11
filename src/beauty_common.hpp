#pragma once
#include <functional>
#include <vector>

#include "header.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

using requestHandlerCallback = std::function<bool(const Request &req, Reply &rep)>;
using fileNotFoundHandlerCallback = std::function<void(Reply &rep)>;
using addFileHeaderCallback = std::function<void(std::vector<Header> &headers)>;

}  // namespace server
}  // namespace http
