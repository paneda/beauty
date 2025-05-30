#include "header.hpp"
#include "mime_types.hpp"
#include "request_handler.hpp"

namespace beauty {

namespace {

void defaultFileNotFoundHandler(const Request&, Reply& rep) {
    rep.stockReply(Reply::not_found);
}

}

RequestHandler::RequestHandler(IFileIO *fileIO)
    : fileIO_(fileIO), fileNotFoundCb_(defaultFileNotFoundHandler) {}

void RequestHandler::addRequestHandler(const handlerCallback &cb) {
    requestHandlers_.push_back(cb);
}

void RequestHandler::setFileNotFoundHandler(const handlerCallback &cb) {
    fileNotFoundCb_ = cb;
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
    if (req.method_ == "GET" && rep.filePath_[rep.filePath_.size() - 1] == '/') {
        rep.filePath_ += "index.html";
        rep.fileExtension_ = "html";
    }

    for (const auto &requestHandler_ : requestHandlers_) {
        requestHandler_(req, rep);
        if (rep.returnToClient_) {
            return;
        }
    }

    if (fileIO_ == nullptr) {
        rep.stockReply(Reply::not_implemented);
        return;
    }

    if (req.method_ == "POST") {
        if (rep.multiPartParser_.parseHeader(req)) {
            rep.status_ = Reply::ok;
            rep.isMultiPart_ = true;
            handlePartialWrite(connectionId, req, content, rep);
            return;
        } else {
            rep.stockReply(Reply::bad_request);
            return;
        }

    } else if (req.method_ == "GET") {
        if (openAndReadFile(connectionId, req, rep) > 0) {
            return;
        } else {
            fileNotFoundCb_(req, rep);
            return;
        }
    }

    rep.stockReply(Reply::not_implemented);
}

void RequestHandler::handlePartialRead(unsigned connectionId, const Request &req, Reply &rep) {
    size_t nrReadBytes = readFromFile(connectionId, req, rep);

    if (nrReadBytes < rep.maxContentSize_) {
        rep.finalPart_ = true;
        fileIO_->closeReadFile(std::to_string(connectionId));
    }
}

void RequestHandler::handlePartialWrite(unsigned connectionId,
                                        const Request &req,
                                        std::vector<char> &content,
                                        Reply &rep) {
    std::deque<MultiPartParser::ContentPart> parts;
    MultiPartParser::result_type result = rep.multiPartParser_.parse(content, parts);

    if (result == MultiPartParser::result_type::bad) {
        rep.stockReply(Reply::status_type::bad_request);
        return;
    }

    writeFileParts(connectionId, req, rep, parts);

    if (result == MultiPartParser::result_type::done) {
        rep.multiPartParser_.flush(content, parts);
        writeFileParts(connectionId, req, rep, parts);
    }

    // done with content unless there's 'bad' messages that should be return to
    // client
    if (rep.status_ == Reply::status_type::ok) {
        rep.content_.clear();
    }
}

void RequestHandler::closeFile(unsigned connectionId) {
    if (fileIO_ != nullptr) {
        fileIO_->closeReadFile(std::to_string(connectionId));
    }
}

bool RequestHandler::openAndReadFile(unsigned connectionId, const Request &req, Reply &rep) {
    // open the file to send back
    size_t contentSize = fileIO_->openFileForRead(std::to_string(connectionId), req, rep);
    if (contentSize > 0) {
        // fill initial content
        rep.replyPartial_ = contentSize > rep.maxContentSize_;
        rep.status_ = Reply::ok;
        readFromFile(connectionId, req, rep);
        if (!rep.replyPartial_) {
            // all data fits in initial content
            fileIO_->closeReadFile(std::to_string(connectionId));
        }

        // Content-Length is always set by server
        if (rep.headers_.empty()) {
            rep.headers_.resize(2);
            rep.headers_[0].name_ = "Content-Length";
            rep.headers_[0].value_ = std::to_string(contentSize);
            rep.headers_[1].name_ = "Content-Type";
            rep.headers_[1].value_ = mime_types::extensionToType(rep.fileExtension_);
        } else {
            rep.addHeader("Content-Length", std::to_string(contentSize));
        }
        return true;
    }
    return false;
}

size_t RequestHandler::readFromFile(unsigned connectionId, const Request &req, Reply &rep) {
    rep.content_.resize(rep.maxContentSize_);
    int nrReadBytes = fileIO_->readFile(
        std::to_string(connectionId), req, rep.content_.data(), rep.content_.size());
    rep.content_.resize(nrReadBytes);
    return nrReadBytes;
}

void RequestHandler::writeFileParts(unsigned connectionId,
                                    const Request &req,
                                    Reply &rep,
                                    std::deque<MultiPartParser::ContentPart> &parts) {
    // It seems that most clients first deliver a "headerOnly" part of the multipart
    // asking for confirmation and then in successive request deliver the part
    // data. If so, we handle this nicely here by giving the client an
    // headerOnly response of the openFileForWrite() response. However this
    // requires "peaking" the last part as the MultiPartParser delivers parts
    // one request too late.
    const std::deque<MultiPartParser::ContentPart> &peakParts = rep.multiPartParser_.peakLastPart();
    for (auto &part : peakParts) {
        if (part.headerOnly_ && !part.filename_.empty()) {
            rep.filePath_ = req.requestPath_ + part.filename_;
            std::string err;
            rep.status_ = fileIO_->openFileForWrite(
                rep.filePath_ + std::to_string(connectionId), req, rep, err);
            rep.multiPartCounter_++;
            if (rep.status_ != Reply::status_type::ok &&
                rep.status_ != Reply::status_type::created) {
                rep.content_.insert(rep.content_.begin(), err.begin(), err.end());
                return;
            }
        }
    }

    // This loop do the actual writing of data to files in sucessive order.
    for (auto &part : parts) {
        std::string err;
        // if 'headerOnly' it as already been handled above
        if (part.headerOnly_ && !part.filename_.empty()) {
            std::string filePath = req.requestPath_ + part.filename_;
            rep.lastOpenFileForWriteId_ = filePath + std::to_string(connectionId);
        } else {
            if (!part.filename_.empty()) {
                // In case client did not issue "headerOnly", its OK, we open
                // the file for writing here. However as we are one request too
                // late, the response will be late too.
                rep.filePath_ = req.requestPath_ + part.filename_;
                rep.lastOpenFileForWriteId_ = rep.filePath_ + std::to_string(connectionId);
                rep.status_ = fileIO_->openFileForWrite(rep.lastOpenFileForWriteId_, req, rep, err);
                rep.multiPartCounter_++;
                if (rep.status_ != Reply::status_type::ok &&
                    rep.status_ != Reply::status_type::created) {
                    rep.content_.insert(rep.content_.begin(), err.begin(), err.end());
                    return;
                }
            }
            size_t size = part.end_ - part.start_;
            rep.status_ = fileIO_->writeFile(
                rep.lastOpenFileForWriteId_, req, &(*part.start_), size, part.foundEnd_, err);
            if (rep.status_ != Reply::status_type::ok &&
                rep.status_ != Reply::status_type::created) {
                rep.lastOpenFileForWriteId_.clear();
                rep.content_.insert(rep.content_.begin(), err.begin(), err.end());
                return;
            }
            if (part.foundEnd_) {
                rep.lastOpenFileForWriteId_.clear();
            }
        }
    }
}

}  // namespace beauty
