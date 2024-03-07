#pragma once

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

    explicit RequestHandler(const std::string &fileRoot,
                            IFileHandler *fileHandler,
                            const std::string &routeRoot,
                            IRouteHandler *routeHandler);

    void addHeaderHandler(addHeaderCallback cb);
    void handleRequest(unsigned connectionId, const Request &req, Reply &rep);
    void handleChunk(unsigned connectionId, Reply &rep);
    void closeFile(unsigned connectionId);

   private:
    size_t readChunkFromFile(unsigned connectionId, Reply &rep);
    static bool urlDecode(const std::string &in, std::string &out);

    // Directory containing the files to be served.
    std::string fileRoot_;

    // Provided FileHandler to be implemented by each specific projects.
    IFileHandler *fileHandler_ = nullptr;

    // Root for provided RouteHandler.
    std::string routeRoot_;

    // Provided RouteHandler to be implemented by each specific projects.
    IRouteHandler *routeHandler_ = nullptr;

    // Callback to add custom headers.
    addHeaderCallback addHeaderCb_;
};

}  // namespace server
}  // namespace http
