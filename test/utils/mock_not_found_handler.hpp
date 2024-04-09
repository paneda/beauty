#pragma once

#include "reply.hpp"
#include "request.hpp"

class MockNotFoundHandler {
   public:
    MockNotFoundHandler() = default;
    virtual ~MockNotFoundHandler() = default;

    void handleNotFound(const http::server::Request &req, http::server::Reply &rep) {
        noCalls_++;
        rep.content_.insert(rep.content_.begin(), mockedContent_.begin(), mockedContent_.end());
        rep.send(http::server::Reply::ok, "text/plain");
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
