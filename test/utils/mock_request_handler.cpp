#include "mock_request_handler.hpp"

namespace http {
namespace server {

void MockRequestHandler::handleRequest(const Request& req, Reply& rep) {
    noCalls_++;
	receivedRequest_ = req;
	receivedReply_ = rep;
    if (returnToClient_) {
        if (!mockedContent_.empty()) {
            // handle reply and return to client
            rep.content_.insert(rep.content_.begin(), mockedContent_.begin(), mockedContent_.end());

            rep.addHeader("Content-Length", std::to_string(mockedContent_.size()));
            rep.addHeader("Content-Type", "text/plain");
        }

        rep.send(status_);
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
