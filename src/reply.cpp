#include <string>

#include "beauty/reply.hpp"

namespace beauty {

namespace status_strings {

const std::string switching_protocols = "HTTP/1.1 101 Switching Protocols\r\n";
const std::string ok = "HTTP/1.1 200 OK\r\n";
const std::string created = "HTTP/1.1 201 Created\r\n";
const std::string accepted = "HTTP/1.1 202 Accepted\r\n";
const std::string no_content = "HTTP/1.1 204 No Content\r\n";
const std::string multiple_choices = "HTTP/1.1 300 Multiple Choices\r\n";
const std::string moved_permanently = "HTTP/1.1 301 Moved Permanently\r\n";
const std::string moved_temporarily = "HTTP/1.1 302 Moved Temporarily\r\n";
const std::string not_modified = "HTTP/1.1 304 Not Modified\r\n";
const std::string bad_request = "HTTP/1.1 400 Bad Request\r\n";
const std::string unauthorized = "HTTP/1.1 401 Unauthorized\r\n";
const std::string forbidden = "HTTP/1.1 403 Forbidden\r\n";
const std::string not_found = "HTTP/1.1 404 Not Found\r\n";
const std::string method_not_allowed = "HTTP/1.1 405 Method Not Allowed\r\n";
const std::string conflict = "HTTP/1.1 409 Conflict\r\n";
const std::string gone = "HTTP/1.1 410 Gone\r\n";
const std::string length_required = "HTTP/1.1 411 Length Required\r\n";
const std::string precondition_failed = "HTTP/1.1 412 Precondition Failed\r\n";
const std::string payload_too_large = "HTTP/1.1 413 Payload Too Large\r\n";
const std::string expectation_failed = "HTTP/1.1 417 Expectation Failed\r\n";
const std::string internal_server_error = "HTTP/1.1 500 Internal Server Error\r\n";
const std::string not_implemented = "HTTP/1.1 501 Not Implemented\r\n";
const std::string bad_gateway = "HTTP/1.1 502 Bad Gateway\r\n";
const std::string service_unavailable = "HTTP/1.1 503 Service Unavailable\r\n";
const std::string version_not_supported = "HTTP/1.1 505 Version Not Supported\r\n";

asio::const_buffer toBuffer(Reply::status_type status) {
    switch (status) {
        case Reply::switching_protocols:
            return asio::buffer(switching_protocols);
        case Reply::ok:
            return asio::buffer(ok);
        case Reply::created:
            return asio::buffer(created);
        case Reply::accepted:
            return asio::buffer(accepted);
        case Reply::no_content:
            return asio::buffer(no_content);
        case Reply::multiple_choices:
            return asio::buffer(multiple_choices);
        case Reply::moved_permanently:
            return asio::buffer(moved_permanently);
        case Reply::moved_temporarily:
            return asio::buffer(moved_temporarily);
        case Reply::not_modified:
            return asio::buffer(not_modified);
        case Reply::bad_request:
            return asio::buffer(bad_request);
        case Reply::unauthorized:
            return asio::buffer(unauthorized);
        case Reply::forbidden:
            return asio::buffer(forbidden);
        case Reply::not_found:
            return asio::buffer(not_found);
        case Reply::method_not_allowed:
            return asio::buffer(method_not_allowed);
        case Reply::conflict:
            return asio::buffer(conflict);
        case Reply::gone:
            return asio::buffer(gone);
        case Reply::length_required:
            return asio::buffer(length_required);
        case Reply::precondition_failed:
            return asio::buffer(precondition_failed);
        case Reply::payload_too_large:
            return asio::buffer(payload_too_large);
        case Reply::expectation_failed:
            return asio::buffer(expectation_failed);
        case Reply::internal_server_error:
            return asio::buffer(internal_server_error);
        case Reply::not_implemented:
            return asio::buffer(not_implemented);
        case Reply::bad_gateway:
            return asio::buffer(bad_gateway);
        case Reply::service_unavailable:
            return asio::buffer(service_unavailable);
        case Reply::version_not_supported:
            return asio::buffer(version_not_supported);
        default:
            return asio::buffer(internal_server_error);
    }
}

}  // namespace status_strings

namespace misc_strings {

const char name_value_separator[] = {':', ' '};
const char crlf[] = {'\r', '\n'};

}  // namespace misc_strings

Reply::Reply(std::vector<char>& content)
    : content_(content), status_(status_type::ok), multiPartParser_(content_) {
    headers_.reserve(2);
}

void Reply::addHeader(const std::string& name, const std::string& val) {
    headers_.push_back({name, val});
}

bool Reply::hasHeaders() const {
    return !headers_.empty();
}

void Reply::send(status_type status) {
    status_ = status;

    if (status < 200 || status == no_content) {
        content_.clear();
    } else {
        headers_.push_back({"Content-Length", "0"});
    }

    returnToClient_ = true;
}

void Reply::send(status_type status, const std::string& contentType) {
    status_ = status;
    headers_.push_back({"Content-Length", std::to_string(content_.size())});
    headers_.push_back({"Content-Type", contentType});

    returnToClient_ = true;
}

void Reply::sendPtr(status_type status,
                    const std::string& contentType,
                    const char* data,
                    size_t size) {
    status_ = status;
    headers_.push_back({"Content-Length", std::to_string(size)});
    headers_.push_back({"Content-Type", contentType});

    contentPtr_ = data;
    contentSize_ = size;

    returnToClient_ = true;
}

std::vector<asio::const_buffer> Reply::headerToBuffers() {
    std::vector<asio::const_buffer> buffers;

    buffers.push_back(status_strings::toBuffer(status_));
    for (std::size_t i = 0; i < headers_.size(); ++i) {
        Header& h = headers_[i];
        buffers.push_back(asio::buffer(h.name_));
        buffers.push_back(asio::buffer(misc_strings::name_value_separator));
        buffers.push_back(asio::buffer(h.value_));
        buffers.push_back(asio::buffer(misc_strings::crlf));
    }
    buffers.push_back(asio::buffer(misc_strings::crlf));
    return buffers;
}

std::vector<asio::const_buffer> Reply::contentToBuffers() {
    std::vector<asio::const_buffer> buffers;
    if (contentPtr_ != nullptr) {
        buffers.push_back(asio::buffer(contentPtr_, contentSize_));
    } else {
        buffers.push_back(asio::buffer(content_));
    }
    return buffers;
}

namespace stock_replies {
// JSON error bodies
const char ok[] = R"({"status":200,"message":"OK"})";
const char created[] = R"({"status":201,"message":"Created"})";
const char accepted[] = R"({"status":202,"message":"Accepted"})";
const char no_content[] = R"({"status":204,"message":"No Content"})";
const char multiple_choices[] = R"({"status":300,"message":"Multiple Choices"})";
const char moved_permanently[] = R"({"status":301,"message":"Moved Permanently"})";
const char moved_temporarily[] = R"({"status":302,"message":"Moved Temporarily"})";
const char not_modified[] = R"({"status":304,"message":"Not Modified"})";
const char bad_request[] = R"({"status":400,"message":"Bad Request"})";
const char unauthorized[] = R"({"status":401,"message":"Unauthorized"})";
const char forbidden[] = R"({"status":403,"message":"Forbidden"})";
const char not_found[] = R"({"status":404,"message":"Not Found"})";
const char length_required[] = R"({"status":411,"message":"Length Required"})";
const char payload_too_large[] = R"({"status":413,"message":"Payload Too Large"})";
const char expectation_failed[] = R"({"status":417,"message":"Expectation Failed"})";
const char internal_server_error[] = R"({"status":500,"message":"Internal Server Error"})";
const char not_implemented[] = R"({"status":501,"message":"Not Implemented"})";
const char bad_gateway[] = R"({"status":502,"message":"Bad Gateway"})";
const char service_unavailable[] = R"({"status":503,"message":"Service Unavailable"})";
const char version_not_supported[] = R"({"status":505,"message":"Version Not Supported"})";

std::vector<char> toArray(Reply::status_type status) {
    switch (status) {
        case Reply::ok:
            return std::vector<char>(ok, ok + sizeof(ok));
        case Reply::created:
            return std::vector<char>(created, created + sizeof(created));
        case Reply::accepted:
            return std::vector<char>(accepted, accepted + sizeof(accepted));
        case Reply::no_content:
            return std::vector<char>(no_content, no_content + sizeof(no_content));
        case Reply::multiple_choices:
            return std::vector<char>(multiple_choices, multiple_choices + sizeof(multiple_choices));
        case Reply::moved_permanently:
            return std::vector<char>(moved_permanently,
                                     moved_permanently + sizeof(moved_permanently));
        case Reply::moved_temporarily:
            return std::vector<char>(moved_temporarily,
                                     moved_temporarily + sizeof(moved_temporarily));
        case Reply::not_modified:
            return std::vector<char>(not_modified, not_modified + sizeof(not_modified));
        case Reply::bad_request:
            return std::vector<char>(bad_request, bad_request + sizeof(bad_request));
        case Reply::unauthorized:
            return std::vector<char>(unauthorized, unauthorized + sizeof(unauthorized));
        case Reply::forbidden:
            return std::vector<char>(forbidden, forbidden + sizeof(forbidden));
        case Reply::not_found:
            return std::vector<char>(not_found, not_found + sizeof(not_found));
        case Reply::length_required:
            return std::vector<char>(length_required, length_required + sizeof(length_required));
        case Reply::payload_too_large:
            return std::vector<char>(payload_too_large,
                                     payload_too_large + sizeof(payload_too_large));
        case Reply::expectation_failed:
            return std::vector<char>(expectation_failed,
                                     expectation_failed + sizeof(expectation_failed));
        case Reply::internal_server_error:
            return std::vector<char>(internal_server_error,
                                     internal_server_error + sizeof(internal_server_error));
        case Reply::not_implemented:
            return std::vector<char>(not_implemented, not_implemented + sizeof(not_implemented));
        case Reply::bad_gateway:
            return std::vector<char>(bad_gateway, bad_gateway + sizeof(bad_gateway));
        case Reply::service_unavailable:
            return std::vector<char>(service_unavailable,
                                     service_unavailable + sizeof(service_unavailable));
        case Reply::version_not_supported:
            return std::vector<char>(version_not_supported,
                                     version_not_supported + sizeof(version_not_supported));
        default:
            return std::vector<char>(internal_server_error,
                                     internal_server_error + sizeof(internal_server_error));
    }
}

}  // namespace stock_replies

void Reply::stockReply(const Request& req, Reply::status_type status) {
    status_ = status;
    content_ = stock_replies::toArray(status);
    headers_.clear();
    if (status_ == no_content) {
        content_.clear();
    } else {
        addHeader("Content-Length", std::to_string(content_.size()));
    }
    addHeader("Content-Type", "application/json");

    if (!isStatusOk()) {
        addHeader("Connection", "close");
    }

    if (req.method_ == "HEAD") {
        content_.clear();
    }
    returnToClient_ = true;
}

}  // namespace beauty
