#include <string>
#include <FS.h>
#include <LittleFS.h>

#include "my_file_api.hpp"
#include "http_result.hpp"
#include "mime_types.hpp"

using namespace http::server;

MyFileApi::MyFileApi(const std::string &docRoot) : docRoot_(docRoot) {}

void MyFileApi::handleRequest(const Request &req, Reply &rep) {
    HttpResult res(rep.content_);
    if (req.method_ == "GET") {
        if (req.startsWith("/list-files")) {
            // json format the response body
            res << "[ ";
            File root = LittleFS.open("/");
            File foundFile = root.openNextFile();
            while (foundFile) {
                std::string filename = std::string(foundFile.name());
                res << "{\"name\":\"" << filename
                    << "\",\"size\":" << std::to_string(foundFile.size()) << "},";
                foundFile = root.openNextFile();
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
                        if (!LittleFS.exists(filename.c_str()) {
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
