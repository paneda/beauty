#include "file_handler.hpp"

#include <limits>

namespace http {
namespace server {

bool FileHandler::openFile(unsigned id, const std::string &path) {
    OpenFile &openFile = openFiles_[id];
    openFile.is_.open(path.c_str(), std::ios::in | std::ios::binary);
    openFile.is_.ignore(std::numeric_limits<std::streamsize>::max());
    openFile.fileSize_ = openFile.is_.gcount();
    openFile.is_.clear();  //  since ignore will have set eof.
    openFile.is_.seekg(0, std::ios_base::beg);
    if (openFile.is_.is_open()) {
        return true;
    }
    openFiles_.erase(id);
    return false;
}

void FileHandler::closeFile(unsigned id) {
    openFiles_[id].is_.close();
    openFiles_.erase(id);
}

size_t FileHandler::getFileSize(unsigned id) {
    return openFiles_[id].fileSize_;
}

int FileHandler::readFile(unsigned id, char *buf, size_t maxSize) {
    openFiles_[id].is_.read(buf, maxSize);
    return openFiles_[id].is_.gcount();
}

}  // namespace server
}  // namespace http
