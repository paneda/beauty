#include <cstdint>
#include <LittleFS.h>

#include "beauty/http_result.hpp"
#include "file_io.hpp"

using namespace beauty;

namespace {
// Assumes that the LittleFS partition is already mounted and labeled
// "littlefs"
const std::string rootPath = "/littlefs/";

}  // namespace

size_t FileIO::openFileForRead(const std::string& id, const Request& request, Reply& reply) {
    HttpResult res(reply.content_);
    const std::string fullPath = rootPath + reply.filePath_;
    openReadFiles_[id] = LittleFS.open(fullPath.c_str(), "r");
    if (!openReadFiles_[id]) {
        res.jsonError(Reply::not_found, "Could not read file: " + reply.filePath_);
        reply.send(res.statusCode_, "application/json");
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

void FileIO::openFileForWrite(const std::string& id, const Request& request, Reply& reply) {
    HttpResult res(reply.content_);
    // Filenames must be max 31 characters long and only under root "/"
    if (reply.filePath_.size() > 31) {
        res.jsonError(Reply::bad_request, "Filename too long max 31 characters are allowed");
        reply.send(res.statusCode_, "application/json");
        return;
    }

    const std::string fullPath = rootPath + reply.filePath_;
    openWriteFiles_[id] = LittleFS.open(fullPath.c_str(), "w");
    if (!openWriteFiles_[id]) {
        openWriteFiles_.erase(id);

        res.jsonError(Reply::internal_server_error,
                      "Could not open file for writing: " + reply.filePath_);
        reply.send(res.statusCode_, "application/json");
        return;
    }
}

void FileIO::writeFile(const std::string& id,
                       const Request& request,
                       Reply& reply,
                       const char* buf,
                       size_t size,
                       bool lastData) {
    openWriteFiles_[id].write((const uint8_t*)buf, size);
    if (lastData) {
        openWriteFiles_[id].close();
        openWriteFiles_.erase(id);

        reply.send(Reply::status_type::created);
    }
}
