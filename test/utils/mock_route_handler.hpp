#pragma once

#include <string>
#include <vector>

#include "i_route_handler.hpp"
#include "reply.hpp"

namespace http {
namespace server {

class MockRouteHandler : public IRouteHandler {
   public:
    MockRouteHandler() = default;
    virtual ~MockRouteHandler() = default;

    virtual void handleRoute(const std::string& path,
                             Reply& reply,
                             std::string& contentType) override;

    void setMockedReponseStatus(const Reply::status_type& status);
    void setMockedContent(const std::string& content);

   private:
    Reply::status_type status_;
    std::string content_;
};

}  // namespace server
}  // namespace http
