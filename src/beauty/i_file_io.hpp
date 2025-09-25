#pragma once

#include <string>

#include "beauty/reply.hpp"
#include "beauty/request.hpp"

namespace beauty {

class IFileIO {
   public:
    IFileIO() = default;
    virtual ~IFileIO() = default;

    virtual size_t openFileForRead(const std::string& id, const Request& request, Reply& reply) = 0;
    virtual int readFile(const std::string& id,
                         const Request& request,
                         char* buf,
                         size_t maxSize) = 0;
    virtual void closeReadFile(const std::string& id) = 0;

    virtual void openFileForWrite(const std::string& id, const Request& request, Reply& reply) = 0;
    virtual void writeFile(const std::string& id,
                           const Request& request,
                           Reply& reply,
                           const char* buf,
                           size_t size,
                           bool lastData) = 0;
};

}  // namespace beauty
