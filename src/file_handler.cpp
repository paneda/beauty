#include "file_handler.hpp"

#include <limits>

namespace http {
namespace server {

bool FileHandler::openFile(std::string path) {
    is_.open(path.c_str(), std::ios::in | std::ios::binary);
    is_.ignore(std::numeric_limits<std::streamsize>::max());
    fileSize_ = is_.gcount();
    is_.clear();  //  since ignore will have set eof.
    is_.seekg(0, std::ios_base::beg);
    return is_.is_open();
}

void FileHandler::closeFile() {
    is_.close();
    fileSize_ = 0;
}

size_t FileHandler::getFileSize() {
    return fileSize_;
}

int FileHandler::readFile(char* buf, size_t maxSize) {
    is_.read(buf, maxSize);
    return is_.gcount();
}

}  // namespace server
}  // namespace http
