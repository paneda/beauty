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

bool MockFileHandler::openFile(unsigned id, const std::string& path) {
    OpenFile& openFile = openFiles_[id];
    if (openFile.isOpen_) {
        throw std::runtime_error("MockFileHandler test error: File already opened");
    }

    if (ends_with(path, "gz")) {
        if (mockFailToOpenGzFile_) {
            openFiles_.erase(id);
            return false;
        }
    } else if (mockFailToOpenRequestedFile_) {
        openFiles_.erase(id);
        return false;
    }
    openFile.readIt_ = mockFileData_.begin();
    openFile.isOpen_ = true;
    openFile.fileSize_ = mockFileData_.size();
    countOpenFileCalls_++;
    return openFile.isOpen_;
}

void MockFileHandler::closeFile(unsigned id) {
    if (!openFiles_[id].isOpen_) {
        throw std::runtime_error("MockFileHandler test error: closeFile() called on closed file");
    }
    countCloseFileCalls_++;
    openFiles_.erase(id);
}

size_t MockFileHandler::getFileSize(unsigned id) {
    if (!openFiles_[id].isOpen_) {
        throw std::runtime_error("MockFileHandler test error: getFileSize() called on closed file");
    }
    return openFiles_[id].fileSize_;
}

int MockFileHandler::readFile(unsigned id, char* buf, size_t maxSize) {
    if (!openFiles_[id].isOpen_) {
        throw std::runtime_error("MockFileHandler test error: readFile() called on closed file");
    }
    OpenFile& openFile = openFiles_[id];
    countReadFileCalls_++;
    size_t leftBytes = std::distance(openFile.readIt_, mockFileData_.end());
    size_t bytesToCopy = std::min(maxSize, leftBytes);
    std::copy(openFile.readIt_, std::next(openFile.readIt_, bytesToCopy), buf);
    std::advance(openFile.readIt_, bytesToCopy);
    return bytesToCopy;
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

void MockFileHandler::setMockFailToOpenRequestedFile() {
    mockFailToOpenRequestedFile_ = true;
}

void MockFileHandler::setMockFailToOpenGzFile() {
    mockFailToOpenGzFile_ = true;
}

int MockFileHandler::getOpenFileCalls() {
    return countOpenFileCalls_;
}

int MockFileHandler::getReadFileCalls() {
    return countReadFileCalls_;
}

int MockFileHandler::getCloseFileCalls() {
    return countCloseFileCalls_;
}
