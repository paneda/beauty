#pragma once

#include <deque>
#include <string>

#include "beauty_common.hpp"
#include "multipart_parser.hpp"

namespace http {
namespace server {

class IFileHandler;
class IRouteHandler;
struct Reply;
struct Request;

class RequestHandler {
   public:
    RequestHandler(const RequestHandler &) = delete;
    RequestHandler &operator=(const RequestHandler &) = delete;

    explicit RequestHandler(IFileHandler *fileHandler);

    void addRequestHandler(const requestHandlerCallback &cb);
    void setFileNotFoundHandler(const fileNotFoundHandlerCallback &cb);
    void addFileHeaderHandler(const addFileHeaderCallback &cb);
    void handleRequest(unsigned connectionId,
                       const Request &req,
                       std::vector<char> &content,
                       Reply &rep);
    void handlePartialRead(unsigned connectionId, Reply &rep);
    void handlePartialWrite(unsigned connectionId,
                            const Request &req,
                            std::vector<char> &content,
                            Reply &rep);
    void closeFile(Reply &rep, unsigned connectionId);

   private:
    bool openAndReadFile(unsigned connectionId, const Request &req, Reply &rep);
    size_t readFromFile(unsigned connectionId, Reply &rep);
    void writeFileParts(unsigned connectionId,
                        const Request &req,
                        Reply &rep,
                        std::deque<MultiPartParser::ContentPart> &parts);

    // Provided FileHandler to be implemented by each specific projects.
    IFileHandler *fileHandler_ = nullptr;

    // Added request handler callbacks
    std::deque<requestHandlerCallback> requestHandlers_;

    // Callback to handle post file access, e.g. a custom not found handler.
    fileNotFoundHandlerCallback fileNotFoundCb_;

    // Callback to add custom http headers for returned files.
    addFileHeaderCallback addFileHeaderCallback_;
};

}  // namespace server
}  // namespace http
