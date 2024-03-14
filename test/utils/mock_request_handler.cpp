#include "mock_request_handler.hpp"

namespace http {
namespace server {

void MockRequestHandler::handleRequest(const Request& req, Reply& rep) {
    noCalls_++;
    receivedRequest_ = req;
    receivedReply_ = rep;
    if (returnToClient_) {
        if (!mockedContent_.empty()) {
            rep.sendPtr(status_, "text/plain", &mockedContent_[0], mockedContent_.size());
        } else {
            rep.send(status_);
        }
    }
}

void MockRequestHandler::setReturnToClient(bool ret) {
    returnToClient_ = ret;
}

void MockRequestHandler::setMockedReply(Reply::status_type status, const std::string& content) {
    status_ = status;
    mockedContent_ = content;
}

int MockRequestHandler::getNoCalls() {
    return noCalls_;
}

Request MockRequestHandler::getReceivedRequest() const {
    return receivedRequest_;
}

Reply MockRequestHandler::getReceivedReply() const {
    return receivedReply_;
}

}  // namespace server
}  // namespace http
