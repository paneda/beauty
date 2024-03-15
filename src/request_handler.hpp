#pragma once

#include <deque>
#include <string>

#include "beauty_common.hpp"

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
    void handleRequest(unsigned connectionId, const Request &req, Reply &rep);
    void handleChunk(unsigned connectionId, Reply &rep);
    void closeFile(unsigned connectionId);

   private:
    size_t readChunkFromFile(unsigned connectionId, Reply &rep);
    static bool urlDecode(const std::string &in, std::string &path, std::string &query);
    static void keyValDecode(const std::string &in,
                             std::vector<std::pair<std::string, std::string>> &params);

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
