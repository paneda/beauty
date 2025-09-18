#pragma once
#include <FS.h>
#include <unordered_map>
#include <beauty/i_file_io.hpp>

// A FileIO example using LittleFS for embedded systems like ESP32/ESP8266.
// This class implements the bare minimum to work with Beauty.
// A more complete example can be found in the PC version of FileIO.
// Make sure LittleFS is properly initialized in your application before use.
class FileIO : public beauty::IFileIO {
   public:
    FileIO() = default;
    virtual ~FileIO() = default;

    FileIO(const FileIO&) = delete;
    FileIO& operator=(const FileIO&) = delete;

    size_t openFileForRead(const std::string& id,
                           const beauty::Request& request,
                           beauty::Reply& reply) override;
    int readFile(const std::string& id,
                 const beauty::Request& request,
                 char* buf,
                 size_t maxSize) override;
    void closeReadFile(const std::string& id) override;

    void openFileForWrite(const std::string& id,
                          const Request& request,
                          beauty::Reply& reply) override;
    void writeFile(const std::string& id,
                   const beauty::Request& request,
                   beauty::Reply& reply,
                   const char* buf,
                   size_t size,
                   bool lastData) override;

   private:
    // As we need to handle multiple connections that reads/writes different
    // files, we keep maps to handle this.
    // Key is the id of each file, provided by Beauty.
    std::unordered_map<std::string, File> openReadFiles_;
    std::unordered_map<std::string, File> openWriteFiles_;
};
