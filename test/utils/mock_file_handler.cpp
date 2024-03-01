
#include "mock_file_handler.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include "file_handler.hpp"

bool MockFileHandler::openFile(std::string path) {
    countOpenFileCalls_++;
    if (isOpen_) {
        throw std::runtime_error("MockFileHandler test error: File already opened");
    }
    isOpen_ = true;
    readIt_ = mockFileData_.begin();
    return !mockFailToOpenFile_;
}

void MockFileHandler::closeFile() {
    if (!isOpen_) {
        throw std::runtime_error("MockFileHandler test error: File already close");
    }
    countCloseFileCalls_++;
    readIt_ = mockFileData_.begin();
}

int MockFileHandler::readFile(char* buf, size_t maxSize) {
    countReadFileCalls_++;
    size_t leftBytes = std::distance(readIt_, mockFileData_.end());
    size_t bytesToCopy = std::min(maxSize, leftBytes);
    std::copy(readIt_, std::next(readIt_, bytesToCopy), buf);
    std::advance(readIt_, bytesToCopy);
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
    readIt_ = mockFileData_.begin();

    int count = 0;
    for (size_t i = 0; i < mockFileData_.size(); i += typeSize) {
        std::memcpy(&mockFileData_.at(i), &count, typeSize);
        count++;
    }
}

void MockFileHandler::setMockFailToOpenFile() {
    mockFailToOpenFile_ = true;
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
