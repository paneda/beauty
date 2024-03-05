
#include "mock_file_handler.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include "file_handler.hpp"

bool MockFileHandler::openFile(unsigned id, const std::string& path) {
    OpenFile& openFile = openFiles_[id];
    openFile.countOpenFileCalls_++;
    if (openFile.isOpen_) {
        throw std::runtime_error("MockFileHandler test error: File already opened");
    }
    openFile.isOpen_ = true;
    openFile.readIt_ = openFile.mockFileData_.begin();
    return !openFile.mockFailToOpenFile_;
}

void MockFileHandler::closeFile(unsigned id) {
    OpenFile& openFile = openFiles_[id];
    if (!openFile.isOpen_) {
        throw std::runtime_error("MockFileHandler test error: File already close");
    }
    openFile.countCloseFileCalls_++;
    openFile.readIt_ = openFile.mockFileData_.begin();
}

size_t MockFileHandler::getFileSize(unsigned id) {
    return openFiles_[id].mockFileData_.size();
}

int MockFileHandler::readFile(unsigned id, char* buf, size_t maxSize) {
    OpenFile& openFile = openFiles_[id];
    openFile.countReadFileCalls_++;
    size_t leftBytes = std::distance(openFile.readIt_, openFile.mockFileData_.end());
    size_t bytesToCopy = std::min(maxSize, leftBytes);
    std::copy(openFile.readIt_, std::next(openFile.readIt_, bytesToCopy), buf);
    std::advance(openFile.readIt_, bytesToCopy);
    return bytesToCopy;
}

// creates and fills the "file" with counter values
void MockFileHandler::createMockFile(unsigned id, uint32_t size) {
    int typeSize = sizeof(size);
    if (size % typeSize != 0) {
        throw std::runtime_error("MockFileHandler setup error: size must be even multiple of " +
                                 std::to_string(typeSize) +
                                 "to accomodate counter "
                                 "values");
    }
    OpenFile& openFile = openFiles_[id];
    openFile.mockFileData_.resize(size);
    openFile.readIt_ = openFile.mockFileData_.begin();

    int count = 0;
    for (size_t i = 0; i < openFile.mockFileData_.size(); i += typeSize) {
        std::memcpy(&openFile.mockFileData_.at(i), &count, typeSize);
        count++;
    }
}

void MockFileHandler::setMockFailToOpenFile(unsigned id) {
    openFiles_[id].mockFailToOpenFile_ = true;
}

int MockFileHandler::getOpenFileCalls(unsigned id) {
    return openFiles_[id].countOpenFileCalls_;
}

int MockFileHandler::getReadFileCalls(unsigned id) {
    return openFiles_[id].countReadFileCalls_;
}

int MockFileHandler::getCloseFileCalls(unsigned id) {
    return openFiles_[id].countCloseFileCalls_;
}
