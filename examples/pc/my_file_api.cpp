#include <filesystem>
#include <string>

#include "my_file_api.hpp"
#include "http_result.hpp"
#include "mime_types.hpp"

namespace fs = std::filesystem;
using namespace beauty;

MyFileApi::MyFileApi(const std::string &docRoot) : docRoot_(docRoot) {}

void MyFileApi::handleRequest(const Request &req, Reply &rep) {
    HttpResult res(rep.content_);
    if (req.method_ == "GET") {
        if (req.startsWith("/list-files")) {
            // json format the response body
            res << "[ ";
            for (const auto &entry : fs::directory_iterator(docRoot_)) {
                if (entry.is_regular_file()) {
                    res << "{\"name\":\"" << entry.path().filename()
                        << "\",\"size\":" << std::to_string(entry.file_size()) << "},";
                }
            }
            // replace last ',' with ']'
            rep.content_[rep.content_.size() - 1] = ']';

            // As send() is invoked, no further calls to other middleware
            // or FileSystem will be done.
            rep.send(res.statusCode_, "application/json");
            return;
        }

        if (req.startsWith("/download-file")) {
            const std::string filename = req.getQueryParam("name").value_;
            if (!fs::exists(docRoot_ + "/" + filename)) {
                res.setError(Reply::status_type::bad_request, "File does not exist");

                // As send() is invoked, no further calls to other middleware
                // or FileSystem will be done.
                rep.send(res.statusCode_, "application/json");
                return;
            }

            // By using addHeader(), we control Content-Type and other headers.
            rep.addHeader("Content-Type", "application/octet-stream");
            rep.addHeader("Content-Disposition", "attachment; filename=" + filename);
            // Set rep.filePath_ to filename so the FileSystem finds it later
            rep.filePath_ = "/" + filename;
            // Just return and let FileSystem read and return the file data
            // from disk.
            return;
        } else {
            // Here we can apply behaviour when file are served as part of
            // our web application.
            // In this example, all docRoot_ files are gzipped, add .gz to path
            // so the FileSystem finds them.
            rep.filePath_ += ".gz";
            // By using addHeader(), we control Content-Type and other headers.
            // Note: As we are adding special response headers for file access,
            // this middleware should be placed last among other middlewares in
            // our project so it runs just before the FileSystem is called.
            rep.addHeader("Content-Type", mime_types::extensionToType(rep.fileExtension_));
            rep.addHeader("Content-Encoding", "gzip");
            // Just return and let FileSystem read and return the file
            // data from disk.
            return;
        }
    }
}
