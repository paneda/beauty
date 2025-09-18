#include "mock_file_io.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include "file_io.hpp"

size_t MockFileIO::openFileForRead(const std::string& id,
                                   const beauty::Request&,
                                   beauty::Reply& reply) {
    OpenReadFile& openFile = openReadFiles_[id];
    countOpenFileForReadCalls_++;
    if (openFile.isOpen_) {
        throw std::runtime_error("MockFileIO test error: File already opened");
    }

    if (mockFailToOpenReadFile_) {
        openReadFiles_.erase(id);
        std::string errMsg = "MockFileIO test error: simulated failure to open file for read";
        reply.content_.insert(reply.content_.begin(), errMsg.begin(), errMsg.end());
        reply.send(beauty::Reply::status_type::not_found, "text/plain");
        return 0;
    }

    for (const auto& header : headers_) {
        reply.addHeader(header.name_, header.value_);
    }

    openFile.readIt_ = mockFileData_.begin();
    openFile.isOpen_ = true;
    return mockFileData_.size();
}

int MockFileIO::readFile(const std::string& id, const beauty::Request&, char* buf, size_t maxSize) {
    OpenReadFile& openFile = openReadFiles_[id];
    if (!openFile.isOpen_) {
        throw std::runtime_error("MockFileIO test error: readFile() called on closed file");
    }
    countReadFileCalls_++;
    size_t leftBytes = std::distance(openFile.readIt_, mockFileData_.end());
    size_t bytesToCopy = std::min(maxSize, leftBytes);
    std::copy(openFile.readIt_, std::next(openFile.readIt_, bytesToCopy), buf);
    std::advance(openFile.readIt_, bytesToCopy);
    return bytesToCopy;
}

void MockFileIO::closeReadFile(const std::string& id) {
    countCloseReadFileCalls_++;
    openReadFiles_.erase(id);
}

void MockFileIO::openFileForWrite(const std::string& id,
                                  const beauty::Request&,
                                  beauty::Reply& reply) {
    OpenWriteFile& openFile = openWriteFiles_[id];
    if (openFile.isOpen_) {
        throw std::runtime_error("MockFileIO test error: File already opened");
    }
    countOpenFileForWriteCalls_++;
    if (mockFailToOpenWriteFile_) {
        openWriteFiles_.erase(id);
        const std::string errMsg =
            "MockFileIO test error: simulated failure to open file for write";
        reply.content_.insert(reply.content_.begin(), errMsg.begin(), errMsg.end());
        reply.send(beauty::Reply::status_type::internal_server_error, "text/plain");
    }

    for (const auto& header : headers_) {
        reply.addHeader(header.name_, header.value_);
    }

    openFile.isOpen_ = true;
}

void MockFileIO::writeFile(const std::string& id,
                           const beauty::Request&,
                           beauty::Reply& reply,
                           const char* buf,
                           size_t size,
                           bool lastData) {
    OpenWriteFile& openFile = openWriteFiles_[id];
    if (!openFile.isOpen_) {
        throw std::runtime_error("MockFileIO test error: writeFile() called on closed file");
    }
    openFile.file_.insert(openFile.file_.end(), buf, buf + size);
    openFile.lastData_ = lastData;
    if (lastData) {
        reply.send(beauty::Reply::status_type::created);
    }
}

int MockFileIO::getOpenFileForWriteCalls() {
    return countOpenFileForWriteCalls_;
}

// creates and fills the "file" with counter values
void MockFileIO::createMockFile(uint32_t size) {
    int typeSize = sizeof(size);
    if (size % typeSize != 0) {
        throw std::runtime_error("MockFileIO setup error: size must be even multiple of " +
                                 std::to_string(typeSize) +
                                 "to accomodate counter "
                                 "values");
    }
    mockFileData_.resize(size);

    int count = 0;
    for (size_t i = 0; i < mockFileData_.size(); i += typeSize) {
        std::memcpy(&mockFileData_.at(i), &count, typeSize);
        count++;
    }
}

std::vector<char> MockFileIO::getMockWriteFile(const std::string& id) {
    return openWriteFiles_[id].file_;
}

void MockFileIO::addHeader(const beauty::Header& header) {
    headers_.push_back(header);
}

void MockFileIO::setMockFailToOpenReadFile() {
    mockFailToOpenReadFile_ = true;
}

void MockFileIO::setMockFailToOpenWriteFile() {
    mockFailToOpenWriteFile_ = true;
}

int MockFileIO::getOpenFileForReadCalls() {
    return countOpenFileForReadCalls_;
}

int MockFileIO::getReadFileCalls() {
    return countReadFileCalls_;
}

int MockFileIO::getCloseReadFileCalls() {
    return countCloseReadFileCalls_;
}

bool MockFileIO::getLastData(const std::string& id) {
    return openWriteFiles_[id].lastData_;
}
