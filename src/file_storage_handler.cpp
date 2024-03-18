#include "file_storage_handler.hpp"

#include <filesystem>
#include <iostream>
#include <string>

#include "http_result2.hpp"

namespace fs = std::filesystem;

namespace {

bool startsWith(const std::string &s, const std::string &sv) {
    return s.rfind(sv, 0) == 0;
}

}

namespace http {
namespace server {

FileStorageHandler::FileStorageHandler(const std::string &docRoot) : docRoot_(docRoot) {
    // load all regular files in docRoot
    for (const auto &entry : fs::directory_iterator(docRoot)) {
        if (entry.is_regular_file()) {
            files_[entry.path().filename()] = entry.file_size();
        }
    }
}

void FileStorageHandler::handleRequest(const Request &req, Reply &rep) {
    HttpResult2 res(rep.content_);
    std::cout << req.requestPath_ << std::endl;
    if (startsWith(req.requestPath_, "/list-files")) {
        res << "[";
        for (const auto &file : files_) {
            res << "{\"name\":\"" << file.first << "\",\"size\":" << std::to_string(file.second)
                << "},";
        }
        // replace last ',' with ']'
        rep.content_[rep.content_.size() - 1] = ']';
        rep.send(res.statusCode_, "application/json");
        return;
    }
    if (startsWith(req.requestPath_, "/file-storage")) {
        auto used = 0;
        for (const auto &file : files_) {
            used += file.second;
        }

        res << "{\"total\":100000,"
            << "\"used\":" << std::to_string(used) << "}";
        rep.send(res.statusCode_, "application/json");
        return;
    }
}

}  // namespace server
}  // namespace http
