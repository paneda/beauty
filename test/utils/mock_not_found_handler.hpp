#pragma once

#include "reply.hpp"
#include "request.hpp"

class MockNotFoundHandler {
   public:
    MockNotFoundHandler() = default;
    virtual ~MockNotFoundHandler() = default;

    void handleNotFound(http::server::Reply &rep) {
        noCalls_++;
        rep.status_ = http::server::Reply::ok;
        rep.content_.insert(rep.content_.begin(), mockedContent_.begin(), mockedContent_.end());

        rep.headers_.resize(2);
        rep.headers_[0].name = "Content-Length";
        rep.headers_[0].value = std::to_string(mockedContent_.size());
        rep.headers_[1].name = "Content-Type";
        rep.headers_[1].value = "text/plain";
    }

    void setMockedContent(const std::string &mockedContent) {
        mockedContent_ = mockedContent;
    }

    int getNoCalls() {
        return noCalls_;
    }

   private:
    int noCalls_ = 0;
    std::string mockedContent_;
};
