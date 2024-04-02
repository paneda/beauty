#include "mock_file_handler.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include "file_handler.hpp"

namespace {
bool ends_with(std::string const& value, std::string const& ending) {
    if (ending.size() > value.size()) {
        return false;
    };
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}
}

size_t MockFileHandler::openFileForRead(const std::string& id, const std::string& path) {
    OpenReadFile& openFile = openReadFiles_[id];
    countOpenFileForReadCalls_++;
    if (openFile.isOpen_) {
        throw std::runtime_error("MockFileHandler test error: File already opened");
    }

    if (mockFailToOpenReadFile_) {
        openReadFiles_.erase(id);
        return 0;
    }
    openFile.readIt_ = mockFileData_.begin();
    openFile.isOpen_ = true;
    return mockFileData_.size();
}

int MockFileHandler::readFile(const std::string& id, char* buf, size_t maxSize) {
    OpenReadFile& openFile = openReadFiles_[id];
    if (!openFile.isOpen_) {
        throw std::runtime_error("MockFileHandler test error: readFile() called on closed file");
    }
    countReadFileCalls_++;
    size_t leftBytes = std::distance(openFile.readIt_, mockFileData_.end());
    size_t bytesToCopy = std::min(maxSize, leftBytes);
    std::copy(openFile.readIt_, std::next(openFile.readIt_, bytesToCopy), buf);
    std::advance(openFile.readIt_, bytesToCopy);
    return bytesToCopy;
}

void MockFileHandler::closeReadFile(const std::string& id) {
    countCloseReadFileCalls_++;
    openReadFiles_.erase(id);
}

http::server::Reply::status_type MockFileHandler::openFileForWrite(const std::string& id,
                                                                   const std::string& path,
                                                                   std::string& err) {
    OpenWriteFile& openFile = openWriteFiles_[id];
    if (openFile.isOpen_) {
        throw std::runtime_error("MockFileHandler test error: File already opened");
    }
    countOpenFileForWriteCalls_++;
    if (mockFailToOpenWriteFile_) {
        openWriteFiles_.erase(id);
        return http::server::Reply::status_type::internal_server_error;
    }
    openFile.isOpen_ = true;
    return http::server::Reply::status_type::created;
}

http::server::Reply::status_type MockFileHandler::writeFile(const std::string& id,
                                                            const char* buf,
                                                            size_t size,
															bool lastData,
                                                            std::string& err) {
    OpenWriteFile& openFile = openWriteFiles_[id];
    if (!openFile.isOpen_) {
        throw std::runtime_error("MockFileHandler test error: writeFile() called on closed file");
    }
    openFile.file_.insert(openFile.file_.end(), buf, buf + size);
	openFile.lastData_ = lastData;
    return http::server::Reply::status_type::ok;
}

int MockFileHandler::getOpenFileForWriteCalls() {
    return countOpenFileForWriteCalls_;
}

// creates and fills the "file" with counter values
void MockFileHandler::createMockFile(uint32_t size) {
    int typeSize = sizeof(size);
    if (size % typeSize != 0) {
        throw std::runtime_error("MockFileHandler setup error: size must be even multiple of " +
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

std::vector<char> MockFileHandler::getMockWriteFile(const std::string& id) {
    return openWriteFiles_[id].file_;
}

void MockFileHandler::setMockFailToOpenReadFile() {
    mockFailToOpenReadFile_ = true;
}

void MockFileHandler::setMockFailToOpenWriteFile() {
    mockFailToOpenWriteFile_ = true;
}

int MockFileHandler::getOpenFileForReadCalls() {
    return countOpenFileForReadCalls_;
}

int MockFileHandler::getReadFileCalls() {
    return countReadFileCalls_;
}

int MockFileHandler::getCloseReadFileCalls() {
    return countCloseReadFileCalls_;
}

bool MockFileHandler::getLastData(const std::string& id) {
    return openWriteFiles_[id].lastData_;
}
