#pragma once
#include <fstream>
#include <unordered_map>

#include "i_file_io.hpp"

class FileIO : public beauty::IFileIO {
   public:
    FileIO(const std::string &docRoot);
    virtual ~FileIO() = default;

    size_t openFileForRead(const std::string &id,
                           const beauty::Request &request,
                           beauty::Reply &reply) override;
    int readFile(const std::string &id,
                 const beauty::Request &request,
                 char *buf,
                 size_t maxSize) override;

    beauty::Reply::status_type openFileForWrite(const std::string &id,
                                                const beauty::Request &request,
                                                beauty::Reply &reply,
                                                std::string &err) override;
    void closeReadFile(const std::string &id) override;

    beauty::Reply::status_type writeFile(const std::string &id,
                                         const beauty::Request &request,
                                         const char *buf,
                                         size_t size,
                                         bool lastData,
                                         std::string &err) override;

   private:
    const std::string docRoot_;

    // As we need to handle multiple connections that reads/writes different
    // files, we keep maps to handle this.
    // Key is the id of each file, provided by Beauty.
    std::unordered_map<std::string, std::ifstream> openReadFiles_;
    std::unordered_map<std::string, std::ofstream> openWriteFiles_;
};
