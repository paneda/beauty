#pragma once
#include <fstream>
#include <string>

#include "i_file_handler.hpp"

namespace http {
namespace server {

class FileHandler : public IFileHandler {
   public:
    FileHandler() = default;
    virtual ~FileHandler() = default;

    virtual bool openFile(std::string path) override;
    virtual void closeFile() override;
    virtual int readFile(char* buf, size_t maxSize) override;

   private:
    std::ifstream is_;
};

}  // namespace server
}  // namespace http
