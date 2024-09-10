#pragma once
#include <FS.h>

#include <unordered_map>

#include "i_file_io.hpp"

class FileIO : public http::server::IFileIO {
   public:
    FileIO() = default;
    virtual ~FileIO() = default;

    FileIO(const FileIO&) = delete;
    FileIO& operator=(const FileIO&) = delete;

    size_t openFileForRead(const std::string& id,
                           const http::server::Request& request,
                           http::server::Reply& reply) override;
    int readFile(const std::string& id,
                 const http::server::Request& request,
                 char* buf,
                 size_t maxSize) override;
    void closeReadFile(const std::string& id) override;

    http::server::Reply::status_type openFileForWrite(const std::string& id,
                                                      const Request& request,
                                                      Reply& reply,
                                                      std::string& err) override;
    http::server::Reply::status_type writeFile(const std::string& id,
                                               const http::server::Request& request,
                                               const char* buf,
                                               size_t size,
                                               bool lastData,
                                               std::string& err) override;

   private:
	// As we need to handle multiple connections that reads/writes different
	// files, we keep maps to handle this.
	// Key is the id of each file, provided by Beauty.
    std::unordered_map<std::string, File> openReadFiles_;
    std::unordered_map<std::string, File> openWriteFiles_;
};
