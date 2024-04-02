#pragma once

#include <map>
#include <string>

#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

class FileStorageHandler {
   public:
    FileStorageHandler(const std::string &docRoot);
    virtual ~FileStorageHandler() = default;

    void handleRequest(const Request &req, Reply &rep);

   private:
    const std::string docRoot_;
    std::map<std::string, size_t> files_;
};

}  // namespace server
}  // namespace http
