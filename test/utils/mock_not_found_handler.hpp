#pragma once

#include "reply.hpp"
#include "request.hpp"

class MockNotFoundHandler {
   public:
    MockNotFoundHandler() = default;
    virtual ~MockNotFoundHandler() = default;

    void handleNotFound(http::server::Reply &rep) {
        noCalls_++;
        rep.content_.insert(rep.content_.begin(), mockedContent_.begin(), mockedContent_.end());

        rep.addHeader("Content-Length", std::to_string(mockedContent_.size()));
        rep.addHeader("Content-Type", "text/plain");
        rep.send(http::server::Reply::ok);
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
