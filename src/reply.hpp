#pragma once
// clang-format off
#include "environment.hpp"
// clang-format on

#include <asio.hpp>
#include <string>
#include <vector>

#include "header.hpp"

namespace http {
namespace server {

class RequestHandler;

class Reply {
    friend class RequestHandler;
    friend class Connection;

   public:
    enum status_type {
        ok = 200,
        created = 201,
        accepted = 202,
        no_content = 204,
        multiple_choices = 300,
        moved_permanently = 301,
        moved_temporarily = 302,
        not_modified = 304,
        bad_request = 400,
        unauthorized = 401,
        forbidden = 403,
        not_found = 404,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503
    } status_;

    // content to be sent in the reply.
    std::vector<char> content_;

    // helper to provide standard replies
    static Reply stockReply(status_type status);

   private:
    // headers to be included in the reply.
    std::vector<Header> headers_;

    // for http chunking (using content-length, not "http chunking")
    bool useChunking_ = false;
    bool finalChunk_ = false;

    // Convert the reply into a vector of buffers. The buffers do not own the
    // underlying memory blocks, therefore the reply object must remain valid
    // and not be changed until the write operation has completed.
    std::vector<asio::const_buffer> headerToBuffers();
    std::vector<asio::const_buffer> contentToBuffers();
};

}  // namespace server
}  // namespace http
