#include "request_handler.hpp"

#include <iostream>
#include <sstream>
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
    // decode url to path
    std::string queryStr;
    if (!urlDecode(req.uri_, rep.requestPath_, queryStr)) {
        rep = Reply::stockReply(Reply::bad_request);
        return;
    }
    // request path must be absolute and not contain ".."
    if (rep.requestPath_.empty() || rep.requestPath_[0] != '/' ||
        rep.requestPath_.find("..") != std::string::npos) {
        rep = Reply::stockReply(Reply::bad_request);
        return;
    }

    keyValDecode(queryStr, rep.queryParams_);

    // if path ends in slash (i.e. is a directory) then add "index.html"
    if (rep.requestPath_[rep.requestPath_.size() - 1] == '/') {
        rep.requestPath_ += "index.html";
    }

    // initiate filePath with requestPath
    rep.filePath_ = rep.requestPath_;

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
            std::size_t lastSlashPos = rep.requestPath_.find_last_of("/");
            std::size_t lastDotPos = rep.requestPath_.find_last_of(".");
            if (lastDotPos != std::string::npos && lastDotPos > lastSlashPos) {
                extension = rep.requestPath_.substr(lastDotPos + 1);
            }

            // fill initial content
            rep.useChunking_ = contentSize > MaxChunkSize;
            rep.status_ = Reply::ok;
            readChunkFromFile(connectionId, rep);
            if (!rep.useChunking_) {
                // all data fits in initial content
                fileHandler_->closeFile(connectionId);
            }

            rep.headers_.resize(2);
            rep.headers_[0].name_ = "Content-Length";
            rep.headers_[0].value_ = std::to_string(contentSize);
            rep.headers_[1].name_ = "Content-Type";
            rep.headers_[1].value_ = mime_types::extensionToType(extension);
            addFileHeaderCallback_(rep.headers_);
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

bool RequestHandler::urlDecode(const std::string &in, std::string &path, std::string &query) {
    path.clear();
    path.reserve(in.size());
    query.clear();
    query.reserve(in.size());
    auto out = &path;
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%') {
            if (i + 3 <= in.size()) {
                int value = 0;
                std::istringstream is(in.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    *out += static_cast<char>(value);
                    i += 2;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        } else if (in[i] == '+') {
            *out += ' ';
        } else if (in[i] == '?') {
            out = &query;
        } else {
            *out += in[i];
        }
    }
    return true;
}

void RequestHandler::keyValDecode(const std::string &in,
                                  std::vector<std::pair<std::string, std::string>> &params) {
    std::string key, val;
    auto out = &key;
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '=') {
            out = &val;
        } else if (in[i] == '&') {
            params.push_back({key, val});
            out = &key;
            key.clear();
            val.clear();
        } else if (i == in.size() - 1) {
            *out += in[i];
            params.push_back({key, val});
        } else {
            *out += in[i];
        }
    }
}

}  // namespace server
}  // namespace http
