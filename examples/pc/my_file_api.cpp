#include <filesystem>
#include <string>
#include <beauty/http_result.hpp>
#include <beauty/mime_types.hpp>

#include "my_file_api.hpp"

namespace fs = std::filesystem;
using namespace beauty;

MyFileApi::MyFileApi(const std::string &docRoot) : docRoot_(docRoot) {}

void MyFileApi::handleRequest(const Request &req, Reply &rep) {
    HttpResult res(rep.content_);
    // Note: For HEAD requests, Beauty will clear rep.content_ before sending
    if (req.method_ == "GET" || req.method_ == "HEAD") {
        if (req.startsWith("/list-files")) {
            res.buildJsonResponse([&]() -> cJSON * {
                cJSON *fileArray = cJSON_CreateArray();
                for (const auto &entry : fs::directory_iterator(docRoot_)) {
                    if (entry.is_regular_file()) {
                        cJSON *fileObj = cJSON_CreateObject();
                        cJSON_AddStringToObject(
                            fileObj, "name", entry.path().filename().string().c_str());
                        cJSON_AddNumberToObject(fileObj, "size", entry.file_size());
                        cJSON_AddItemToArray(fileArray, fileObj);
                    }
                }
                return fileArray;
            });

            // As send() is invoked, no further calls to other middleware
            // or FileIO will be done.
            rep.send(res.statusCode_, "application/json");
            return;
        }

        if (req.startsWith("/download-file")) {
            const std::string filename = req.getQueryParam("name").value_;
            // By using addHeader(), we control Content-Type and other headers.
            rep.addHeader("Content-Type", "application/octet-stream");
            rep.addHeader("Content-Disposition", "attachment; filename=" + filename);
            // Set rep.filePath_ to filename so the FileIO finds it later
            rep.filePath_ = filename;
            // Just return and let FileIO read and return the file data
            // from disk.
            return;
        }
    }
}
