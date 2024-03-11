#include "mock_request_handler.hpp"

#include "reply.hpp"

namespace http {
namespace server {

bool MockRequestHandler::handleRequest(const Request& req, Reply& rep) {
    noCalls_++;
    if (!retVal_) {
        if (!mockedContent_.empty()) {
            // handle reply and return to client
            rep.status_ = status_;
            rep.content_.insert(rep.content_.begin(), mockedContent_.begin(), mockedContent_.end());

            rep.headers_.resize(2);
            rep.headers_[0].name = "Content-Length";
            rep.headers_[0].value = std::to_string(mockedContent_.size());
            rep.headers_[1].name = "Content-Type";
            rep.headers_[1].value = "text/plain";
        }

        return false;
    }
    // continue to next handler
    return true;
}

void MockRequestHandler::setMockedReturnVal(bool retVal) {
    retVal_ = retVal;
}

void MockRequestHandler::setMockedReply(Reply::status_type status, const std::string& content) {
    status_ = status;
    mockedContent_ = content;
}

int MockRequestHandler::getNoCalls() {
    return noCalls_;
}

}  // namespace server
}  // namespace http
