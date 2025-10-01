#include "mock_request_handler.hpp"

namespace beauty {

MockRequestHandler::MockRequestHandler(std::vector<char>& body)
    : sendBuffer_(1024), receivedRequest_(body), receivedReply_(sendBuffer_) {}

void MockRequestHandler::handleRequest(const Request& req, Reply& rep) {
    noCalls_++;
    receivedRequest_.method_ = req.method_;
    receivedRequest_.uri_ = req.uri_;
    receivedRequest_.httpVersionMajor_ = req.httpVersionMajor_;
    receivedRequest_.httpVersionMinor_ = req.httpVersionMinor_;
    receivedRequest_.headers_ = req.headers_;
    receivedRequest_.keepAlive_ = req.keepAlive_;
    receivedRequest_.requestPath_ = req.requestPath_;
    receivedRequest_.body_ = req.body_;
    receivedRequest_.queryParams_ = req.queryParams_;
    receivedRequest_.formParams_ = req.formParams_;

    receivedReply_.content_ = rep.content_;
    receivedReply_.filePath_ = rep.filePath_;

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

Reply& MockRequestHandler::getReceivedReply() {
    return receivedReply_;
}

}  // namespace beauty
