#pragma once
#include <fstream>
#include <unordered_map>

#include "i_file_handler.hpp"

namespace http {
namespace server {

class FileHandler : public IFileHandler {
   public:
    FileHandler() = default;
    virtual ~FileHandler() = default;

    virtual bool openFile(unsigned id, const std::string &path) override;
    virtual void closeFile(unsigned id) override;
    virtual size_t getFileSize(unsigned id) override;
    virtual int readFile(unsigned id, char *buf, size_t maxSize) override;

   private:
    struct OpenFile {
        std::ifstream is_;
        size_t fileSize_ = 0;
    };
    std::unordered_map<unsigned, OpenFile> openFiles_;
};

}  // namespace server
}  // namespace http
