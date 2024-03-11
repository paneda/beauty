#pragma once

#include <string>
#include <vector>

#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

class MockRequestHandler {
   public:
    MockRequestHandler() = default;
    virtual ~MockRequestHandler() = default;

    bool handleRequest(const Request& req, Reply& rep);

    void setMockedReturnVal(bool retVal);
    void setMockedReply(Reply::status_type status, const std::string& content);
    int getNoCalls();

   private:
    int noCalls_ = 0;
    bool retVal_ = true;
    Reply::status_type status_ = Reply::status_type::internal_server_error;
    std::string mockedContent_;
};

}  // namespace server
}  // namespace http
