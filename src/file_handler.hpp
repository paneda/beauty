#pragma once
#include <fstream>
#include <unordered_map>

#include "i_file_handler.hpp"

namespace http {
namespace server {

class FileHandler : public IFileHandler {
   public:
    FileHandler(const std::string &docRoot);
    virtual ~FileHandler() = default;

    virtual size_t openFileForRead(const std::string &id, const std::string &path) override;
    virtual void closeFile(const std::string &id) override;
    virtual int readFile(const std::string &id, char *buf, size_t maxSize) override;
    virtual Reply::status_type openFileForWrite(const std::string &id,
                                                const std::string &path,
                                                std::string &err) override;
    virtual Reply::status_type writeFile(const std::string &id,
                                         const char *buf,
                                         size_t size,
                                         std::string &err) override;

   private:
    const std::string docRoot_;
    std::unordered_map<std::string, std::ifstream> openReadFiles_;
    std::unordered_map<std::string, std::ofstream> openWriteFiles_;
};

}  // namespace server
}  // namespace http
