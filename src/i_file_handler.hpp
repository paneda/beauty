#pragma once

#include <string>

namespace http {
namespace server {

class IFileHandler {
   public:
    IFileHandler() = default;
    virtual ~IFileHandler() = default;

    virtual bool openFile(std::string path) = 0;
    virtual void closeFile() = 0;
    virtual size_t getFileSize() = 0;
    virtual int readFile(char* buf, size_t maxSize) = 0;
};

}  // namespace server
}  // namespace http
