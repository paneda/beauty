#pragma once
#include <string>

#include "i_route_handler.hpp"
#include "reply.hpp"
#include "route_handler.hpp"

namespace http {
namespace server {

class RouteHandler : public IRouteHandler {
   public:
    RouteHandler() = default;
    virtual ~RouteHandler() = default;

    virtual void handleRoute(const std::string& path,
                             Reply& reply,
                             std::string& contentType) override;
};
}  // namespace server
}  // namespace http
