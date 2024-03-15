#include "file_handler.hpp"

#include <limits>

namespace http {
namespace server {

FileHandler::FileHandler(const std::string &docRoot) : docRoot_(docRoot) {}

size_t FileHandler::openFile(unsigned id, const std::string &path) {
    std::string fullPath = docRoot_ + path;
    std::ifstream &is = openFiles_[id];
    is.open(fullPath.c_str(), std::ios::in | std::ios::binary);
    is.ignore(std::numeric_limits<std::streamsize>::max());
    size_t fileSize = is.gcount();
    is.clear();  //  since ignore will have set eof.
    is.seekg(0, std::ios_base::beg);
    if (is.is_open()) {
        return fileSize;
    }
    openFiles_.erase(id);
    return 0;
}

void FileHandler::closeFile(unsigned id) {
    openFiles_[id].close();
    openFiles_.erase(id);
}

int FileHandler::readFile(unsigned id, char *buf, size_t maxSize) {
    openFiles_[id].read(buf, maxSize);
    return openFiles_[id].gcount();
}

}  // namespace server
}  // namespace http
