#include "file_io.hpp"

#include <iostream>
#include <limits>

using namespace beauty;

FileIO::FileIO(const std::string &docRoot) : docRoot_(docRoot) {}

size_t FileIO::openFileForRead(const std::string &id, const Request &request, Reply &reply) {
    std::string fullPath = docRoot_ + reply.filePath_;
    std::ifstream &is = openReadFiles_[id];
    is.open(fullPath.c_str(), std::ios::in | std::ios::binary);
    is.ignore(std::numeric_limits<std::streamsize>::max());
    size_t fileSize = is.gcount();
    is.clear();  //  since ignore will have set eof.
    is.seekg(0, std::ios_base::beg);
    if (is.is_open()) {
        return fileSize;
    }
    openReadFiles_.erase(id);
    return 0;
}

int FileIO::readFile(const std::string &id,
                          const Request &request,
                          char *buf,
                          size_t maxSize) {
    openReadFiles_[id].read(buf, maxSize);
    return openReadFiles_[id].gcount();
}

void FileIO::closeReadFile(const std::string &id) {
    openReadFiles_[id].close();
    openReadFiles_.erase(id);
}

Reply::status_type FileIO::openFileForWrite(const std::string &id,
                                                 const Request &request,
                                                 Reply &reply,
                                                 std::string &err) {
    std::string fullPath = docRoot_ + reply.filePath_;
    std::ofstream &os = openWriteFiles_[id];
    os.open(fullPath.c_str(), std::ios::out | std::ios::binary);
    return Reply::status_type::ok;
}

Reply::status_type FileIO::writeFile(const std::string &id,
                                          const Request &request,
                                          const char *buf,
                                          size_t size,
                                          bool lastData,
                                          std::string &err) {
    openWriteFiles_[id].write(buf, size);
    if (lastData) {
        openWriteFiles_[id].close();
		openWriteFiles_.erase(id);
    }
    return Reply::status_type::ok;
}
