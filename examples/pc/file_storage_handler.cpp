#include <filesystem>
#include <string>

#include "file_storage_handler.hpp"
#include "http_result.hpp"

namespace fs = std::filesystem;

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
    HttpResult res(rep.content_);
    if (req.startsWith("/list-files")) {
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
    if (req.startsWith("/file-storage")) {
        auto used = 0;
        for (const auto &file : files_) {
            used += file.second;
        }

        // just a "made up" limit
        res << "{\"total\":100000,"
            << "\"used\":" << std::to_string(used) << "}";
        rep.send(res.statusCode_, "application/json");
        return;
    }
}

}  // namespace server
}  // namespace http
