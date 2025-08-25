#pragma once
#include <FS.h>
#include <unordered_map>
#include <beauty/i_file_io.hpp>

class FileIO : public beauty::IFileIO {
   public:
    FileIO() = default;
    virtual ~FileIO() = default;

    FileIO(const FileIO&) = delete;
    FileIO& operator=(const FileIO&) = delete;

    size_t openFileForRead(const std::string& id,
                           const beauty::Request& request,
                           beauty::Reply& reply) override;
    int readFile(const std::string& id,
                 const beauty::Request& request,
                 char* buf,
                 size_t maxSize) override;
    void closeReadFile(const std::string& id) override;

    beauty::Reply::status_type openFileForWrite(const std::string& id,
                                                const Request& request,
                                                Reply& reply,
                                                std::string& err) override;
    beauty::Reply::status_type writeFile(const std::string& id,
                                         const beauty::Request& request,
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
