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

    virtual size_t openFile(unsigned id, const std::string &path) override;
    virtual void closeFile(unsigned id) override;
    virtual int readFile(unsigned id, char *buf, size_t maxSize) override;

   private:
    std::unordered_map<unsigned, std::ifstream> openFiles_;
};

}  // namespace server
}  // namespace http
