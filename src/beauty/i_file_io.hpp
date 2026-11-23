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

    // Called when an upload is abandoned before writeFile() has been called
    // with lastData == true for a file opened with openFileForWrite(), e.g.
    // because a later part of the same multipart request was rejected. The
    // implementation should close the file identified by id and release any
    // resources held for it; it may also remove the partially written file.
    // Must tolerate an unknown id. The default does nothing, so existing
    // implementations keep compiling, but they should override it to avoid
    // leaking the open file handle.
    virtual void abortWriteFile(const std::string& id) {
        (void)id;
    }
};

}  // namespace beauty
