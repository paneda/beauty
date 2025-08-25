#include <cstdint>
#include <LittleFS.h>

#include "file_io.hpp"

using namespace beauty;

size_t FileIO::openFileForRead(const std::string& id, const Request& request, Reply& reply) {
    openReadFiles_[id] = LittleFS.open(reply.filePath_.c_str(), "r");
    if (!openReadFiles_[id]) {
        openReadFiles_.erase(id);
        return 0;
    }

    return openReadFiles_[id].size();
}

int FileIO::readFile(const std::string& id, const Request& request, char* buf, size_t maxSize) {
    return openReadFiles_[id].readBytes(buf, maxSize);
}

void FileIO::closeReadFile(const std::string& id) {
    openReadFiles_[id].close();
    openReadFiles_.erase(id);
}

Reply::status_type FileIO::openFileForWrite(const std::string& id,
                                            const Request& request,
                                            Reply& reply,
                                            std::string& err) {
    // only support files under '/' so '/' + max filename = 32
    if (reply.filePath_.size() > 32) {
        err = "Filename too long max 31 characters are allowed";
        return Reply::bad_request;
    }

    openWriteFiles_[id] = LittleFS.open(reply.filePath_.c_str(), "w");
    if (!openWriteFiles_[id]) {
        openWriteFiles_.erase(id);
        return Reply::internal_server_error;
    }

    return Reply::ok;
}

Reply::status_type FileIO::writeFile(const std::string& id,
                                     const Request& request,
                                     const char* buf,
                                     size_t size,
                                     bool lastData,
                                     std::string& err) {
    openWriteFiles_[id].write((const uint8_t*)buf, size);
    if (lastData) {
        openWriteFiles_[id].close();
        openWriteFiles_.erase(id);
    }

    return Reply::ok;
}
