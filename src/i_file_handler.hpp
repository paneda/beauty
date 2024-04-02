#pragma once

#include <string>

#include "reply.hpp"

namespace http {
namespace server {

class IFileHandler {
   public:
    IFileHandler() = default;
    virtual ~IFileHandler() = default;

    virtual size_t openFileForRead(const std::string& id, const std::string& path) = 0;
    virtual int readFile(const std::string& id, char* buf, size_t maxSize) = 0;
    virtual void closeReadFile(const std::string& id) = 0;

    virtual Reply::status_type openFileForWrite(const std::string& id,
                                                const std::string& path,
                                                std::string& err) = 0;
    virtual Reply::status_type writeFile(const std::string& id,
                                         const char* buf,
                                         size_t size,
										 bool lastData,
                                         std::string& err) = 0;
};

}  // namespace server
}  // namespace http
