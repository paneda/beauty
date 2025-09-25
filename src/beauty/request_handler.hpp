#pragma once

#include "beauty/beauty_common.hpp"
#include "beauty/multipart_parser.hpp"
#include "beauty/i_file_io.hpp"
#include "beauty/reply.hpp"
#include "beauty/request.hpp"

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
    void setExpectContinueHandler(const handlerCallback &cb);

    void shouldContinueAfterHeaders(const Request &req, Reply &rep);

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
    void openAndReadFile(unsigned connectionId, const Request &req, Reply &rep);
    size_t readFromFile(unsigned connectionId, const Request &req, Reply &rep);
    void writeFileParts(unsigned connectionId,
                        const Request &req,
                        Reply &rep,
                        std::deque<MultiPartParser::ContentPart> &parts);

    static void defaultExpectContinueHandler(const Request &, Reply &rep);

    // Provided FileIO to be implemented by each specific projects.
    IFileIO *fileIO_ = nullptr;

    // Added request handler callbacks
    std::deque<handlerCallback> requestHandlers_;

    // Callback to handle post file access, e.g. a custom not found handler.
    handlerCallback fileNotFoundCb_;

    // Callback to handle Expect: 100-continue requests
    handlerCallback expectContinueCb_;
};

}  // namespace beauty
