#pragma once

#include <string>

#include "reply.hpp"
#include "request.hpp"

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

    virtual Reply::status_type openFileForWrite(const std::string& id,
                                                const Request& request,
                                                Reply& reply,
                                                std::string& err) = 0;
    virtual Reply::status_type writeFile(const std::string& id,
                                         const Request& request,
                                         const char* buf,
                                         size_t size,
                                         bool lastData,
                                         std::string& err) = 0;
};

}  // namespace beauty
