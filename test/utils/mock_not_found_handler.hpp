#pragma once

#include "beauty/reply.hpp"
#include "beauty/request.hpp"

class MockNotFoundHandler {
   public:
    MockNotFoundHandler() = default;
    virtual ~MockNotFoundHandler() = default;

    void handleNotFound(const beauty::Request &, beauty::Reply &rep) {
        noCalls_++;
        rep.content_.insert(rep.content_.begin(), mockedContent_.begin(), mockedContent_.end());
        rep.send(beauty::Reply::ok, "text/plain");
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
