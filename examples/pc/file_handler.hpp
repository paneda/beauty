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

    size_t openFileForRead(const std::string &id, const Request &request, Reply &reply) override;
    int readFile(const std::string &id, const Request &request, char *buf, size_t maxSize) override;

    Reply::status_type openFileForWrite(const std::string &id,
                                        const Request &request,
                                        Reply &reply,
                                        std::string &err) override;
    void closeReadFile(const std::string &id) override;

    Reply::status_type writeFile(const std::string &id,
                                 const Request &request,
                                 const char *buf,
                                 size_t size,
                                 bool lastData,
                                 std::string &err) override;

   private:
    const std::string docRoot_;
    std::unordered_map<std::string, std::ifstream> openReadFiles_;
    std::unordered_map<std::string, std::ofstream> openWriteFiles_;
};

}  // namespace server
}  // namespace http
