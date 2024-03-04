#pragma once
#include <fstream>

#include "i_file_handler.hpp"

namespace http {
namespace server {

class FileHandler : public IFileHandler {
   public:
    FileHandler() = default;
    virtual ~FileHandler() = default;

    virtual bool openFile(std::string path) override;
    virtual void closeFile() override;
    virtual size_t getFileSize() override;
    virtual int readFile(char* buf, size_t maxSize) override;

   private:
    std::ifstream is_;
    size_t fileSize_ = 0;
};

}  // namespace server
}  // namespace http
