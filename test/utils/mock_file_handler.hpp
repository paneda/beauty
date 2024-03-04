#pragma once
#include <stdint.h>

#include <string>
#include <vector>

#include "i_file_handler.hpp"

class MockFileHandler : public http::server::IFileHandler {
   public:
    MockFileHandler() = default;
    virtual ~MockFileHandler() = default;

    virtual bool openFile(std::string path) override;
    virtual void closeFile() override;
    virtual size_t getFileSize() override;
    virtual int readFile(char* buf, size_t maxSize) override;

    void createMockFile(uint32_t size);
    void setMockFailToOpenFile();

    int getOpenFileCalls();
    int getReadFileCalls();
    int getCloseFileCalls();

   private:
    int countOpenFileCalls_ = 0;
    int countReadFileCalls_ = 0;
    int countCloseFileCalls_ = 0;
    bool isOpen_ = false;
    std::vector<char> mockFileData_;
    std::vector<char>::iterator readIt_;
    bool mockFailToOpenFile_ = false;
};

