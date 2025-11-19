#include "beauty/header.hpp"
#include "beauty/mime_types.hpp"
#include "beauty/request_handler.hpp"

namespace beauty {

namespace {
std::string combineUploadPaths(const std::string &dir, const std::string &filename) {
    // Handle cases where slashes may be missing or duplicated.
    if (dir.empty()) {
        return filename;
    }
    if (filename.empty()) {
        return dir;
    }
    if (dir.back() == '/' && filename.front() == '/') {
        return dir + filename.substr(1);
    } else if (dir.back() != '/' && filename.front() != '/') {
        return dir + "/" + filename;
    } else {
        return dir + filename;
    }
}
}  // namespace

RequestHandler::RequestHandler(size_t maxContentSize)
    : maxContentSize_(maxContentSize), expectContinueCb_(defaultExpectContinueHandler) {}

void RequestHandler::defaultExpectContinueHandler(const Request &, Reply &rep) {
    // Default: approve all 100-continue requests
    rep.send(Reply::ok);
}

void RequestHandler::setFileIO(IFileIO *fileIO) {
    fileIO_ = fileIO;
}

void RequestHandler::addRequestHandler(const handlerCallback &cb) {
    requestHandlers_.push_back(cb);
}

void RequestHandler::setExpectContinueHandler(const handlerCallback &cb) {
    expectContinueCb_ = cb;
}

void RequestHandler::shouldContinueAfterHeaders(const Request &req, Reply &rep) {
    expectContinueCb_(req, rep);
}

void RequestHandler::handleRequest(unsigned connectionId,
                                   const Request &req,
                                   std::vector<char> &content,
                                   Reply &rep) {
    // initiate filePath with requestPath
    rep.filePath_ = req.requestPath_;

    // determine the file extension.
    std::size_t lastSlashPos = req.requestPath_.find_last_of("/");
    std::size_t lastDotPos = req.requestPath_.find_last_of(".");
    if (lastDotPos != std::string::npos && lastDotPos > lastSlashPos) {
        rep.fileExtension_ = req.requestPath_.substr(lastDotPos + 1);
    }

    // if path ends in slash (i.e. is a directory) then add "index.html"
    if ((req.method_ == "GET" || req.method_ == "HEAD") &&
        rep.filePath_[rep.filePath_.size() - 1] == '/') {
        rep.filePath_ += "index.html";
        rep.fileExtension_ = "html";
    }

    for (const auto &requestHandler_ : requestHandlers_) {
        requestHandler_(req, rep);
        if (rep.returnToClient_) {
            if (req.method_ == "HEAD") {
                rep.content_.clear();
            }
            return;
        }
    }

    if (fileIO_ == nullptr) {
        rep.stockReply(req, Reply::not_implemented);
        return;
    }

    if (req.method_ == "POST") {
        if (rep.isMultiPart_ || rep.multiPartParser_.parseHeader(req)) {
            rep.status_ = Reply::ok;
            rep.isMultiPart_ = true;
            handleFileIOWrite(connectionId, req, content, rep);
            return;
        } else {
            rep.stockReply(req, Reply::bad_request);
            return;
        }
    } else if (req.method_ == "GET" || req.method_ == "HEAD") {
        openAndReadFile(connectionId, req, rep);
        return;
    }

    rep.stockReply(req, Reply::not_implemented);
}

void RequestHandler::handleStreamingRead(unsigned connectionId, Reply &rep) {
    if (rep.streamCallback_ && !rep.finalPart_) {
        // Calculate safe buffer size for chunked encoding
        size_t callbackBufferSize = maxContentSize_;
        if (rep.useChunkedEncoding_) {
            // Worst case chunk overhead: "FFFFFFFF\r\n" + "\r\n" = 12 bytes for 32-bit size_t
            // But for practical embedded use, assume max 6 hex digits: "100000\r\n" + "\r\n" = 10
            // bytes The min size allowed for maxContentSize_ is 1024 so this is safe
            const size_t maxChunkOverhead = 10;
            callbackBufferSize = maxContentSize_ - maxChunkOverhead;
        }

        rep.content_.resize(callbackBufferSize);
        int bytesRead = rep.streamCallback_(
            std::to_string(connectionId), rep.content_.data(), callbackBufferSize);

        if (bytesRead > 0) {
            rep.content_.resize(bytesRead);
            rep.streamedBytes_ += bytesRead;

            if (rep.useChunkedEncoding_) {
                // Wrap content in chunk format if using chunked encoding
                rep.wrapContentInChunkFormat();
                // For chunked: final when callback returns 0 or negative
                rep.finalPart_ = false;  // Will be set on next iteration when bytesRead <= 0
            } else {
                // For sendBig: final when we've sent all expected bytes or chunk is smaller than
                // buffer
                rep.finalPart_ = (rep.streamedBytes_ >= rep.totalStreamSize_) ||
                                 (bytesRead < static_cast<int>(maxContentSize_));
            }
        } else {
            // End of stream
            rep.finalPart_ = true;
            if (rep.useChunkedEncoding_) {
                // Send final chunk "0\r\n\r\n"
                rep.content_.clear();
                rep.wrapContentInChunkFormat();
            } else {
                rep.content_.clear();
            }
        }
    }
}

void RequestHandler::handleFileIORead(unsigned connectionId, const Request &req, Reply &rep) {
    size_t nrReadBytes = readFromFile(connectionId, req, rep);

    if (nrReadBytes < maxContentSize_) {
        rep.finalPart_ = true;
        fileIO_->closeReadFile(std::to_string(connectionId));
    }
}

void RequestHandler::handleFileIOWrite(unsigned connectionId,
                                       const Request &req,
                                       std::vector<char> &content,
                                       Reply &rep) {
    if (rep.finalPart_) {
        return;
    }

    std::deque<MultiPartParser::ContentPart> parts;
    MultiPartParser::result_type result = rep.multiPartParser_.parse(content, parts);

    if (result == MultiPartParser::result_type::bad) {
        rep.stockReply(req, Reply::status_type::bad_request);
        return;
    }

    writeFileParts(connectionId, req, rep, parts);
    if (!rep.isStatusOk() && rep.status_ != Reply::status_type::no_content) {
        rep.addHeader("Content-Length", std::to_string(rep.content_.size()));
        return;
    }

    if (result == MultiPartParser::result_type::done) {
        rep.multiPartParser_.flush(content, parts);
        writeFileParts(connectionId, req, rep, parts);
    }
}

void RequestHandler::closeFile(unsigned connectionId) {
    if (fileIO_ != nullptr) {
        fileIO_->closeReadFile(std::to_string(connectionId));
    }
}

void RequestHandler::openAndReadFile(unsigned connectionId, const Request &req, Reply &rep) {
    // open the file to send back
    size_t contentSize = fileIO_->openFileForRead(std::to_string(connectionId), req, rep);

    if (rep.isStatusOk()) {
        if (req.method_ == "HEAD") {
            // HEAD request, no content
            rep.content_.clear();
            fileIO_->closeReadFile(std::to_string(connectionId));
        } else {
            // fill initial content
            rep.replyPartial_ = contentSize > maxContentSize_;
            readFromFile(connectionId, req, rep);
            if (!rep.replyPartial_) {
                // all data fits in initial content
                fileIO_->closeReadFile(std::to_string(connectionId));
            }
        }

        // Make sure Content-Length and Content-Type headers are Set
        bool hasContentLength = false;
        bool hasContentType = false;
        for (const auto &header : rep.headers_) {
            if (header.name_ == "Content-Length") {
                hasContentLength = true;
            } else if (header.name_ == "Content-Type") {
                hasContentType = true;
            }
        }

        if (!hasContentLength) {
            rep.headers_.push_back({"Content-Length", std::to_string(contentSize)});
        }
        if (!hasContentType) {
            rep.headers_.push_back(
                {"Content-Type", mime_types::extensionToType(rep.fileExtension_)});
        }
    } else if (rep.status_ == Reply::not_modified) {
        // 304 Not Modified response, no content
    }
}

size_t RequestHandler::readFromFile(unsigned connectionId, const Request &req, Reply &rep) {
    rep.content_.resize(maxContentSize_);
    int nrReadBytes = fileIO_->readFile(
        std::to_string(connectionId), req, rep.content_.data(), rep.content_.size());
    rep.content_.resize(nrReadBytes);
    return nrReadBytes;
}

void RequestHandler::writeFileParts(unsigned connectionId,
                                    const Request &req,
                                    Reply &rep,
                                    std::deque<MultiPartParser::ContentPart> &parts) {
    // It seems that some clients first deliver a "headerOnly" part of the
    // multipart (assumably checking for non successful response) and then in
    // successive request deliver the part data. If so, we handle this nicely
    // here by giving the client an early non-successful reply. However this
    const std::deque<MultiPartParser::ContentPart> &peakParts = rep.multiPartParser_.peakLastPart();
    for (auto &part : peakParts) {
        if (part.headerOnly_ && !part.filename_.empty()) {
            rep.filePath_ = combineUploadPaths(req.requestPath_, part.filename_);
            fileIO_->openFileForWrite(rep.filePath_ + std::to_string(connectionId), req, rep);
            if (!rep.isStatusOk()) {
                return;
            }
        }
    }

    // This loop do the actual writing of data to files in sucessive order.
    for (auto &part : parts) {
        // if 'headerOnly' it as already been handled above
        if (part.headerOnly_ && !part.filename_.empty()) {
            const std::string filePath = combineUploadPaths(req.requestPath_, part.filename_);
            rep.lastOpenFileForWriteId_ = filePath + std::to_string(connectionId);
        } else {
            if (!part.filename_.empty()) {
                // In case client did not issue "headerOnly", its OK, we open
                // the file for writing here. However as we are one request too
                // late, the response will be late too.
                rep.filePath_ = combineUploadPaths(req.requestPath_, part.filename_);
                rep.lastOpenFileForWriteId_ = rep.filePath_ + std::to_string(connectionId);
                fileIO_->openFileForWrite(rep.lastOpenFileForWriteId_, req, rep);
                if (!rep.isStatusOk()) {
                    return;
                }
            }
            size_t size = part.end_ - part.start_;
            fileIO_->writeFile(
                rep.lastOpenFileForWriteId_, req, rep, &(*part.start_), size, part.foundEnd_);
            if (!rep.isStatusOk()) {
                rep.lastOpenFileForWriteId_.clear();
                return;
            }
            if (part.foundEnd_) {
                rep.lastOpenFileForWriteId_.clear();
                rep.finalPart_ = true;
            }
        }
    }
}

}  // namespace beauty
