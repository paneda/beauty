#pragma once
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "beauty/i_file_io.hpp"
#include "beauty/reply.hpp"

class MockFileIO : public beauty::IFileIO {
   public:
    MockFileIO() = default;
    virtual ~MockFileIO() = default;

    size_t openFileForRead(const std::string& id,
                           const beauty::Request& request,
                           beauty::Reply& reply) override;
    int readFile(const std::string& id,
                 const beauty::Request& request,
                 char* buf,
                 size_t maxSize) override;
    void closeReadFile(const std::string& id) override;

    void openFileForWrite(const std::string& id,
                          const beauty::Request& request,
                          beauty::Reply& reply) override;
    void writeFile(const std::string& id,
                   const beauty::Request& request,
                   beauty::Reply& reply,
                   const char* buf,
                   size_t size,
                   bool lastData) override;

    void createMockFile(uint32_t size);
    void setMockFailToOpenReadFile();
    void setMockFailToOpenWriteFile();
    std::vector<char> getMockWriteFile(const std::string& id);
    void addHeader(const beauty::Header& header);

    int getOpenFileForReadCalls();
    int getOpenFileForWriteCalls();
    int getReadFileCalls();
    int getCloseReadFileCalls();
    bool getLastData(const std::string& id);

   private:
    struct OpenReadFile {
        std::vector<char>::iterator readIt_;
        bool isOpen_ = false;
    };
    struct OpenWriteFile {
        std::vector<char> file_;
        bool isOpen_ = false;
        bool lastData_ = false;
    };
    std::unordered_map<std::string, OpenReadFile> openReadFiles_;
    std::unordered_map<std::string, OpenWriteFile> openWriteFiles_;
    std::vector<char> mockFileData_;
    int countOpenFileForReadCalls_ = 0;
    int countOpenFileForWriteCalls_ = 0;
    int countReadFileCalls_ = 0;
    int countCloseReadFileCalls_ = 0;
    bool mockFailToOpenReadFile_ = false;
    bool mockFailToOpenWriteFile_ = false;
    std::vector<beauty::Header> headers_;
};
