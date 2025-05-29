#pragma once

#include "beauty_common.hpp"
#include "multipart_parser.hpp"
#include "i_file_io.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace beauty {

class IRouteHandler;
class Reply;
struct Request;

class RequestHandler {
   public:
    RequestHandler(const RequestHandler &) = delete;
    RequestHandler &operator=(const RequestHandler &) = delete;

    explicit RequestHandler(IFileIO *fileIO);

    // Handlers to be optionally implemented.
    void addRequestHandler(const handlerCallback &cb);
    void setFileNotFoundHandler(const handlerCallback &cb);

    void handleRequest(unsigned connectionId,
                       const Request &req,
                       std::vector<char> &content,
                       Reply &rep);
    void handlePartialRead(unsigned connectionId, const Request &req, Reply &rep);
    void handlePartialWrite(unsigned connectionId,
                            const Request &req,
                            std::vector<char> &content,
                            Reply &rep);
    void closeFile(unsigned connectionId);

   private:
    bool openAndReadFile(unsigned connectionId, const Request &req, Reply &rep);
    size_t readFromFile(unsigned connectionId, const Request &req, Reply &rep);
    void writeFileParts(unsigned connectionId,
                        const Request &req,
                        Reply &rep,
                        std::deque<MultiPartParser::ContentPart> &parts);

    // Provided FileIO to be implemented by each specific projects.
    IFileIO *fileIO_ = nullptr;

    // Added request handler callbacks
    std::deque<handlerCallback> requestHandlers_;

    // Callback to handle post file access, e.g. a custom not found handler.
    handlerCallback fileNotFoundCb_;
};

}  // namespace beauty
