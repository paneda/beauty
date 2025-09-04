#pragma once
#include "beauty/environment.hpp"

#include <asio.hpp>
#include <string>
#include <vector>

#include "beauty/header.hpp"
#include "beauty/multipart_parser.hpp"

namespace beauty {

class RequestHandler;

class Reply {
    friend class RequestHandler;
    friend class Connection;

   public:
    Reply(const Reply&) = delete;
    Reply& operator=(const Reply&) = delete;

    Reply(size_t maxContentSize);
    virtual ~Reply() = default;

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
        method_not_allowed = 405,
        length_required = 411,
        payload_too_large = 413,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503,
        version_not_supported = 505
    };

    // Content to be sent in the reply.
    std::vector<char> content_;

    // File path to open.
    std::string filePath_;

    // Extension of the file to open
    std::string fileExtension_;

    void send(status_type status);
    void send(status_type status, const std::string& contentType);
    void sendPtr(status_type status, const std::string& contentType, const char* data, size_t size);
    void addHeader(const std::string& name, const std::string& val);
    bool hasHeaders() const;

// Test-only interface - enable to access private members for e.g. unit test of middlewares.
#ifdef BEAUTY_ENABLE_TESTING
    status_type getStatus() const {
        return status_;
    }

    const std::vector<Header>& getHeaders() const {
        return headers_;
    }

    static bool iequals(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) {
            return false;
        }

        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }

    std::string getHeaderValue(const std::string& headerName) const {
        for (const auto& header : headers_) {
            if (iequals(header.name_, headerName)) {
                return header.value_;
            }
        }
        return "";
    }
#endif

   private:
    void reset() {
        content_.clear();
        filePath_.clear();
        fileExtension_.clear();
        headers_.clear();
        returnToClient_ = false;
        contentPtr_ = nullptr;
        contentSize_ = 0;
        replyPartial_ = false;
        finalPart_ = false;
        noBodyBytesReceived_ = 0;
        isMultiPart_ = false;
        lastOpenFileForWriteId_ = "";
    }

    // Helper to provide standard server replies.
    void stockReply(status_type status);

    bool isStatusOk() const {
        return status_ == ok || status_ == created || status_ == accepted || status_ == no_content;
    }

    // Headers to be included in the reply.
    status_type status_;
    std::vector<Header> headers_;

    bool returnToClient_ = false;
    const char* contentPtr_ = nullptr;
    size_t contentSize_;

    // The max buffer size when writing socket.
    const size_t maxContentSize_;

    // Keep track when replying with successive write buffers.
    bool replyPartial_ = false;
    bool finalPart_ = false;

    // Keep track of the number of body bytes received in request body.
    size_t noBodyBytesReceived_ = 0;

    // Keep track if the body is a multi-part upload.
    bool isMultiPart_ = false;

    // Keep track of the last opened file in multi-part transfers.
    std::string lastOpenFileForWriteId_;

    // Parser to handle multipart uploads.
    MultiPartParser multiPartParser_;

    // Convert the reply into a vector of buffers. The buffers do not own the
    // underlying memory blocks, therefore the reply object must remain valid
    // and not be changed until the write operation has completed.
    std::vector<asio::const_buffer> headerToBuffers();
    std::vector<asio::const_buffer> contentToBuffers();
};

}  // namespace beauty
