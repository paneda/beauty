#include "request_handler.hpp"

#include <sstream>
#include <string>

#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace {
const size_t MaxChunkSize = 1024;
}

namespace http {
namespace server {

RequestHandler::RequestHandler(const std::string &docRoot, IFileHandler &fileHandler)
    : docRoot_(docRoot), fileHandler_(fileHandler) {}

void RequestHandler::handleRequest(unsigned connectionId, const Request &req, Reply &rep) {
    // Decode url to path.
    std::string requestPath;
    if (!urlDecode(req.uri_, requestPath)) {
        rep = Reply::stockReply(Reply::bad_request);
        return;
    }

    // Request path must be absolute and not contain "..".
    if (requestPath.empty() || requestPath[0] != '/' ||
        requestPath.find("..") != std::string::npos) {
        rep = Reply::stockReply(Reply::bad_request);
        return;
    }

    // If path ends in slash (i.e. is a directory) then add "index.html".
    if (requestPath[requestPath.size() - 1] == '/') {
        requestPath += "index.html";
    }

    // Determine the file extension.
    std::size_t last_slash_pos = requestPath.find_last_of("/");
    std::size_t last_dot_pos = requestPath.find_last_of(".");
    std::string extension;
    if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos) {
        extension = requestPath.substr(last_dot_pos + 1);
    }

    // Open the file to send back.
    std::string fullPath = docRoot_ + requestPath;
    if (!fileHandler_.openFile(connectionId, fullPath)) {
        rep = Reply::stockReply(Reply::not_found);
        return;
    }

    size_t fileSize = fileHandler_.getFileSize(connectionId);
    rep.useChunking_ = fileSize > MaxChunkSize;
    rep.status = Reply::ok;
    // fill initial content
    readChunkFromFile(connectionId, rep);
    if (!rep.useChunking_) {
        // all data fits in initial content
        fileHandler_.closeFile(connectionId);
    }

    rep.headers_.resize(2);
    rep.headers_[0].name = "Content-Length";
    rep.headers_[0].value = std::to_string(fileSize);
    rep.headers_[1].name = "Content-Type";
    rep.headers_[1].value = mime_types::extensionToType(extension);
}

void RequestHandler::handleChunk(unsigned connectionId, Reply &rep) {
    size_t nrReadBytes = readChunkFromFile(connectionId, rep);

    if (nrReadBytes < MaxChunkSize) {
        rep.finalChunk_ = true;
        fileHandler_.closeFile(connectionId);
    }
}

size_t RequestHandler::readChunkFromFile(unsigned connectionId, Reply &rep) {
    rep.content_.resize(MaxChunkSize);
    int nrReadBytes = fileHandler_.readFile(connectionId, rep.content_.data(), rep.content_.size());
    rep.content_.resize(nrReadBytes);
    return nrReadBytes;
}

bool RequestHandler::urlDecode(const std::string &in, std::string &out) {
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if (i + 3 <= in.size()) {
                int value = 0;
                std::istringstream is(in.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    out += static_cast<char>(value);
                    i += 2;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        } else if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return true;
}

}  // namespace server
}  // namespace http
