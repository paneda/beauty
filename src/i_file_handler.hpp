#pragma once

#include <string>

namespace http {
namespace server {

class IFileHandler {
   public:
    IFileHandler() = default;
    virtual ~IFileHandler() = default;

    virtual size_t openFile(unsigned id, const std::string& path) = 0;
    virtual void closeFile(unsigned id) = 0;
    virtual int readFile(unsigned id, char* buf, size_t maxSize) = 0;
};

}  // namespace server
}  // namespace http
