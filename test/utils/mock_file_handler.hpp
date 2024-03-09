#pragma once
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "i_file_handler.hpp"

class MockFileHandler : public http::server::IFileHandler {
   public:
    MockFileHandler() = default;
    virtual ~MockFileHandler() = default;

    virtual size_t openFile(unsigned id, const std::string& path) override;
    virtual void closeFile(unsigned id) override;
    virtual int readFile(unsigned id, char* buf, size_t maxSize) override;

    void createMockFile(uint32_t size);
    void setMockFailToOpenRequestedFile();
    void setMockFailToOpenGzFile();

    int getOpenFileCalls();
    int getReadFileCalls();
    int getCloseFileCalls();

   private:
    struct OpenFile {
        std::vector<char>::iterator readIt_;
        bool isOpen_ = false;
    };
    std::unordered_map<unsigned, OpenFile> openFiles_;
    std::vector<char> mockFileData_;
    int countOpenFileCalls_ = 0;
    int countReadFileCalls_ = 0;
    int countCloseFileCalls_ = 0;
    bool mockFailToOpenRequestedFile_ = false;
    bool mockFailToOpenGzFile_ = false;
};

