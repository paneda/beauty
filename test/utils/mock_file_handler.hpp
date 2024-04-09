#pragma once
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "i_file_handler.hpp"
#include "reply.hpp"

class MockFileHandler : public http::server::IFileHandler {
   public:
    MockFileHandler() = default;
    virtual ~MockFileHandler() = default;

    size_t openFileForRead(const std::string& id,
                           const http::server::Request& request,
                           http::server::Reply& reply) override;
    int readFile(const std::string& id,
                 const http::server::Request& request,
                 char* buf,
                 size_t maxSize) override;
    void closeReadFile(const std::string& id) override;

    http::server::Reply::status_type openFileForWrite(const std::string& id,
                                                      const http::server::Request& request,
                                                      http::server::Reply& reply,
                                                      std::string& err) override;
    http::server::Reply::status_type writeFile(const std::string& id,
                                               const http::server::Request& request,
                                               const char* buf,
                                               size_t size,
                                               bool lastData,
                                               std::string& err) override;

    void createMockFile(uint32_t size);
    void setMockFailToOpenReadFile();
    void setMockFailToOpenWriteFile();
    std::vector<char> getMockWriteFile(const std::string& id);

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
};

