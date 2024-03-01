#include "file_handler.hpp"

#include <sstream>

namespace http {
namespace server {

bool FileHandler::openFile(std::string path) {
    is_.open(path.c_str(), std::ios::in | std::ios::binary);
    return is_.is_open();
}

void FileHandler::closeFile() {
    is_.close();
}

int FileHandler::readFile(char* buf, size_t maxSize) {
    is_.read(buf, maxSize);
    return is_.gcount();
}

}  // namespace server
}  // namespace http
