#include "mock_route_handler.hpp"

namespace http {
namespace server {

void MockRouteHandler::handleRoute(const std::string& path,
                                   Reply& reply,
                                   std::string& contentType) {
    reply.status_ = status_;
    reply.content_.insert(reply.content_.begin(), content_.begin(), content_.end());
    contentType = "application/json";
}

void MockRouteHandler::setMockedReponseStatus(const Reply::status_type& status) {
    status_ = status;
}

void MockRouteHandler::setMockedContent(const std::string& content) {
    content_ = content;
}

}  // namespace server
}  // namespace http
