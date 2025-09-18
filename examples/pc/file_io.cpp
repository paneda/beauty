#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <iomanip>
#include <filesystem>

#include "beauty/http_result.hpp"
#include "beauty/header.hpp"
#include "file_io.hpp"

namespace fs = std::filesystem;
using namespace beauty;

namespace {
// A simple polynomial rolling hash function to generate a hash from content.
// This function updates an existing hash value with new data.
unsigned long long simple_rolling_hash_update(unsigned long long current_hash,
                                              const char *content,
                                              size_t size) {
    const unsigned long long p = 31;       // A prime number
    const unsigned long long m = 1e9 + 9;  // A large prime modulus
    unsigned long long p_pow = 1;

    for (size_t i = 0; i < size; ++i) {
        current_hash = (current_hash + (content[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return current_hash;
}

// Function to generate an ETag from a file's content by reading in chunks.
std::string generate_etag_from_file(const std::string &filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return "";  // Return an empty string if the file cannot be opened.
    }

    // Use a fixed-size buffer for reading chunks to save memory.
    const size_t chunk_size = 1024;
    std::vector<char> buffer(chunk_size);
    unsigned long long final_hash = 0;

    while (file.read(buffer.data(), chunk_size)) {
        final_hash = simple_rolling_hash_update(final_hash, buffer.data(), chunk_size);
    }

    // Process the last chunk, which might be smaller than chunk_size
    size_t last_chunk_size = file.gcount();
    if (last_chunk_size > 0) {
        final_hash = simple_rolling_hash_update(final_hash, buffer.data(), last_chunk_size);
    }

    // Convert the final hash to a hex string for a more compact ETag.
    std::stringstream ss;
    ss << std::hex << final_hash;

    // Return the ETag as a quoted string, which is the required format.
    return "\"" + ss.str() + "\"";
}

}  // namespace

FileIO::FileIO(const std::string &docRoot) : docRoot_(docRoot) {
    // Read existing files and generate their ETags
    // For simplicity, this example does not implement directory traversal.
    for (const auto &entry : fs::directory_iterator(docRoot_)) {
        if (entry.is_regular_file()) {
            std::string fullPath = entry.path().string();
            eTags_[fullPath] = generate_etag_from_file(fullPath);
        }
    }
}

size_t FileIO::openFileForRead(const std::string &id, const Request &req, Reply &reply) {
    HttpResult res(reply.content_);

    // Remove leading slash from filePath_ to make it relative
    if (!reply.filePath_.empty() && reply.filePath_[0] == '/') {
        reply.filePath_ = reply.filePath_.substr(1);
    }

    if (reply.filePath_ == "index.html") {
        reply.addHeader("Cache-Control", "no-cache, no-store, must-revalidate, max-age=0");
    }

    fs::path fullPath = fs::path(docRoot_) / reply.filePath_;

    // Check if file exists and is a regular file
    if (!fs::exists(fullPath)) {
        res.jsonError(Reply::not_found, "Could not read file: " + reply.filePath_);
        reply.send(res.statusCode_, "application/json");
        return 0;
    }

    // Check for If-None-Match header (ETag matching)
    const std::string requestETag = req.getHeaderValue("If-None-Match");

    if (!requestETag.empty()) {
        std::string currentETag = eTags_[fullPath.string()];
        if (!currentETag.empty() && requestETag == currentETag) {
            reply.addHeader("ETag", currentETag);
            reply.send(Reply::not_modified);
            return 0;  // No content to read for 304
        }
    }

    // Get file size using filesystem
    std::error_code ec;
    size_t fileSize = fs::file_size(fullPath, ec);
    if (ec) {
        res.jsonError(Reply::internal_server_error, "Unexpected error occured: " + ec.message());
        reply.send(res.statusCode_, "application/json");
        return 0;  // Error getting file size
    }

    // Open file for reading
    std::ifstream &is = openReadFiles_[id];
    is.open(fullPath, std::ios::in | std::ios::binary);
    if (!is.is_open()) {
        openReadFiles_.erase(id);
        res.jsonError(Reply::internal_server_error, "Could not open file: " + reply.filePath_);
        reply.send(res.statusCode_, "application/json");
        return 0;
    }

    // Add ETag header for successful reads
    auto etag = eTags_.find(fullPath.string());
    if (etag != eTags_.end()) {
        reply.addHeader("ETag", etag->second);
    }

    return fileSize;
}

int FileIO::readFile(const std::string &id, const Request &, char *buf, size_t maxSize) {
    openReadFiles_[id].read(buf, maxSize);
    return openReadFiles_[id].gcount();
}

void FileIO::closeReadFile(const std::string &id) {
    openReadFiles_[id].close();
    openReadFiles_.erase(id);
}

void FileIO::openFileForWrite(const std::string &id, const Request &, Reply &reply) {
    HttpResult res(reply.content_);

    // Remove leading slash from filePath_ to make it relative
    if (!reply.filePath_.empty() && reply.filePath_[0] == '/') {
        reply.filePath_ = reply.filePath_.substr(1);
    }
    fs::path fullPath = fs::path(docRoot_) / reply.filePath_;

    // Ensure parent directory exists
    fs::path parentDir = fullPath.parent_path();
    if (!fs::exists(parentDir)) {
        std::error_code ec;
        fs::create_directories(parentDir, ec);
        if (ec) {
            res.jsonError(Reply::internal_server_error,
                          "Could not create directory: " + parentDir.string());
            reply.send(res.statusCode_, "application/json");
            return;
        }
    }

    std::ofstream &os = openWriteFiles_[id];
    os.open(fullPath, std::ios::out | std::ios::binary);
    if (!os.is_open()) {
        res.jsonError(Reply::internal_server_error,
                      "Could not open file for writing: " + reply.filePath_);
        reply.send(res.statusCode_, "application/json");
    }
}

void FileIO::writeFile(const std::string &id,
                       const Request &,
                       Reply &reply,
                       const char *buf,
                       size_t size,
                       bool lastData) {
    openWriteFiles_[id].write(buf, size);
    if (lastData) {
        openWriteFiles_[id].close();
        openWriteFiles_.erase(id);

        // Regenerate ETag for the updated file
        fs::path fullPath = fs::path(docRoot_) / reply.filePath_;
        if (fs::exists(fullPath) && fs::is_regular_file(fullPath)) {
            eTags_[fullPath.string()] = generate_etag_from_file(fullPath.string());
        }

        reply.send(Reply::status_type::created);
    }
}
