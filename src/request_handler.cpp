#include "request_handler.hpp"

#include <iostream>
#include <string>

#include "beauty_common.hpp"
#include "file_handler.hpp"
#include "header.hpp"
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

namespace {
const size_t MaxChunkSize = 1024;

void defaultFileNotFoundHandler(Reply &rep) {
    rep = Reply::stockReply(Reply::not_found);
}

void defaultAddFileHeaderHandler(std::vector<Header> &headers) {}

bool startsWith(const std::string &s, const std::string &sv) {
    return s.rfind(sv, 0) == 0;
}

}

RequestHandler::RequestHandler(IFileHandler *fileHandler)
    : fileHandler_(fileHandler),
      fileNotFoundCb_(defaultFileNotFoundHandler),
      addFileHeaderCallback_(defaultAddFileHeaderHandler) {}

void RequestHandler::addRequestHandler(const requestHandlerCallback &cb) {
    requestHandlers_.push_back(cb);
}

void RequestHandler::setFileNotFoundHandler(const fileNotFoundHandlerCallback &cb) {
    fileNotFoundCb_ = cb;
}

void RequestHandler::addFileHeaderHandler(const addFileHeaderCallback &cb) {
    addFileHeaderCallback_ = cb;
}

void RequestHandler::handleRequest(unsigned connectionId, const Request &req, Reply &rep) {
    // initiate filePath with requestPath
    rep.filePath_ = req.requestPath_;

    // if path ends in slash (i.e. is a directory) then add "index.html"
    if (rep.filePath_[rep.filePath_.size() - 1] == '/') {
        rep.filePath_ += "index.html";
    }

    for (const auto &requestHandler_ : requestHandlers_) {
        requestHandler_(req, rep);
        if (rep.returnToClient_) {
            return;
        }
    }

    if (fileHandler_ != nullptr) {
        // open the file to send back
        size_t contentSize = fileHandler_->openFile(connectionId, rep.filePath_);
        if (contentSize > 0) {
            // determine the file extension.
            std::string extension;
            // ..again if directory, then extension is provided by index.html
            if (req.requestPath_[req.requestPath_.size() - 1] == '/') {
                extension = "html";
            } else {
                std::size_t lastSlashPos = req.requestPath_.find_last_of("/");
                std::size_t lastDotPos = req.requestPath_.find_last_of(".");
                if (lastDotPos != std::string::npos && lastDotPos > lastSlashPos) {
                    extension = req.requestPath_.substr(lastDotPos + 1);
                }
            }

            // fill initial content
            rep.useChunking_ = contentSize > MaxChunkSize;
            rep.status_ = Reply::ok;
            readChunkFromFile(connectionId, rep);
            if (!rep.useChunking_) {
                // all data fits in initial content
                fileHandler_->closeFile(connectionId);
            }

            rep.defaultHeaders_.resize(2);
            rep.defaultHeaders_[0].name_ = "Content-Length";
            rep.defaultHeaders_[0].value_ = std::to_string(contentSize);
            rep.defaultHeaders_[1].name_ = "Content-Type";
            rep.defaultHeaders_[1].value_ = mime_types::extensionToType(extension);
            addFileHeaderCallback_(rep.addedHeaders_);
            return;
        }
    }

    fileNotFoundCb_(rep);
}

void RequestHandler::handleChunk(unsigned connectionId, Reply &rep) {
    size_t nrReadBytes = readChunkFromFile(connectionId, rep);

    if (nrReadBytes < MaxChunkSize) {
        rep.finalChunk_ = true;
        fileHandler_->closeFile(connectionId);
    }
}

void RequestHandler::closeFile(unsigned connectionId) {
    if (fileHandler_ != nullptr) {
        fileHandler_->closeFile(connectionId);
    }
}

size_t RequestHandler::readChunkFromFile(unsigned connectionId, Reply &rep) {
    rep.content_.resize(MaxChunkSize);
    int nrReadBytes =
        fileHandler_->readFile(connectionId, rep.content_.data(), rep.content_.size());
    rep.content_.resize(nrReadBytes);
    return nrReadBytes;
}

}  // namespace server
}  // namespace http
