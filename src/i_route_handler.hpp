#pragma once

#include <string>
#include <vector>

#include "reply.hpp"

namespace http {
namespace server {

class IRouteHandler {
   public:
    IRouteHandler() = default;
    virtual ~IRouteHandler() = default;

    virtual void handleRoute(const std::string& path, Reply& reply, std::string& contentType) = 0;
};

}  // namespace server
}  // namespace http
