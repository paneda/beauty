#include "request_handler.hpp"

#include <sstream>
#include <string>

#include "file_handler.hpp"
#include "header.hpp"
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"
#include "route_handler.hpp"

namespace {
const size_t MaxChunkSize = 1024;
void defaultAddHeaderCallback(std::vector<http::server::Header> &) {}

bool startsWith(const std::string &s, const std::string &sv) {
    return s.rfind(sv, 0) == 0;
}

}

namespace http {
namespace server {

RequestHandler::RequestHandler(const std::string &fileRoot,
                               IFileHandler *fileHandler,
                               const std::string &routeRoot,
                               IRouteHandler *routeHandler)
    : fileRoot_(fileRoot),
      fileHandler_(fileHandler),
      routeRoot_(routeRoot),
      routeHandler_(routeHandler),
      addHeaderCb_(defaultAddHeaderCallback) {}

void RequestHandler::addHeaderHandler(addHeaderCallback cb) {
    addHeaderCb_ = cb;
}

void RequestHandler::handleRequest(unsigned connectionId, const Request &req, Reply &rep) {
    // decode url to path
    std::string requestPath;
    if (!urlDecode(req.uri_, requestPath)) {
        rep = Reply::stockReply(Reply::bad_request);
        return;
    }

    // request path must be absolute and not contain ".."
    if (requestPath.empty() || requestPath[0] != '/' ||
        requestPath.find("..") != std::string::npos) {
        rep = Reply::stockReply(Reply::bad_request);
        return;
    }

    // if path ends in slash (i.e. is a directory) then add "index.html"
    if (requestPath[requestPath.size() - 1] == '/') {
        requestPath += "index.html";
    }

    if (fileHandler_ == nullptr && routeHandler_ == nullptr) {
        rep = Reply::stockReply(Reply::not_found);
        return;
    }

    std::string contentType;
    size_t contentSize = 0;
    // check route handler first and avoid invoking fileHandler_ if not needed
    bool useRouteHandler = false;
    if (routeHandler_ != nullptr) {
        std::string actualPath = requestPath;
        if (startsWith(requestPath, routeRoot_)) {
            actualPath = requestPath.substr(routeRoot_.size(), std::string::npos);
            routeHandler_->handleRoute(actualPath, rep, contentType);
            contentSize = rep.content_.size();
            useRouteHandler = true;
        }
    }

    bool isGz = false;
    if (!useRouteHandler && fileHandler_ != nullptr) {
        // determine the file extension.
        std::string extension;
        std::size_t lastSlashPos = requestPath.find_last_of("/");
        std::size_t lastDotPos = requestPath.find_last_of(".");
        if (lastDotPos != std::string::npos && lastDotPos > lastSlashPos) {
            extension = requestPath.substr(lastDotPos + 1);
            contentType = mime_types::extensionToType(extension);
        }

        // open the file to send back
        std::string fullPath = fileRoot_ + requestPath;
        // attempt .gz first
        std::string fullPathGz = fullPath + ".gz";
        contentSize = fileHandler_->openFile(connectionId, fullPathGz);
        if (contentSize > 0) {
            isGz = true;
        } else {
            fileHandler_->closeFile(connectionId);
            contentSize = fileHandler_->openFile(connectionId, fullPath);
            if (contentSize == 0) {
                fileHandler_->closeFile(connectionId);
                rep = Reply::stockReply(Reply::not_found);
                return;
            }
        }

        rep.useChunking_ = contentSize > MaxChunkSize;
        rep.status_ = Reply::ok;
        // fill initial content
        readChunkFromFile(connectionId, rep);
        if (!rep.useChunking_) {
            // all data fits in initial content
            fileHandler_->closeFile(connectionId);
        }
    }

    rep.headers_.resize(2);
    rep.headers_[0].name = "Content-Length";
    rep.headers_[0].value = std::to_string(contentSize);
    rep.headers_[1].name = "Content-Type";
    rep.headers_[1].value = contentType;
    if (isGz) {
        rep.headers_.push_back({"Content-Encoding", "gzip"});
    }
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
