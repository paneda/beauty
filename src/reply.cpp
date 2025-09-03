#include "beauty/reply.hpp"

#include <string>

namespace beauty {

namespace status_strings {

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
const std::string length_required = "HTTP/1.1 411 Length Required\r\n";
const std::string internal_server_error = "HTTP/1.1 500 Internal Server Error\r\n";
const std::string not_implemented = "HTTP/1.1 501 Not Implemented\r\n";
const std::string bad_gateway = "HTTP/1.1 502 Bad Gateway\r\n";
const std::string service_unavailable = "HTTP/1.1 503 Service Unavailable\r\n";
const std::string version_not_supported = "HTTP/1.1 505 Version Not Supported\r\n";

asio::const_buffer toBuffer(Reply::status_type status) {
    switch (status) {
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
        case Reply::length_required:
            return asio::buffer(length_required);
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

Reply::Reply(size_t maxContentSize) : maxContentSize_(maxContentSize), multiPartParser_(content_) {
    content_.reserve(maxContentSize);
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
    headers_.push_back({"Content-Length", "0"});

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

const char ok[] = "";
const char created[] =
    "<html>"
    "<head><title>Created</title></head>"
    "<body><h1>201 Created</h1></body>"
    "</html>";
const char accepted[] =
    "<html>"
    "<head><title>Accepted</title></head>"
    "<body><h1>202 Accepted</h1></body>"
    "</html>";
const char no_content[] =
    "<html>"
    "<head><title>No Content</title></head>"
    "<body><h1>204 Content</h1></body>"
    "</html>";
const char multiple_choices[] =
    "<html>"
    "<head><title>Multiple Choices</title></head>"
    "<body><h1>300 Multiple Choices</h1></body>"
    "</html>";
const char moved_permanently[] =
    "<html>"
    "<head><title>Moved Permanently</title></head>"
    "<body><h1>301 Moved Permanently</h1></body>"
    "</html>";
const char moved_temporarily[] =
    "<html>"
    "<head><title>Moved Temporarily</title></head>"
    "<body><h1>302 Moved Temporarily</h1></body>"
    "</html>";
const char not_modified[] =
    "<html>"
    "<head><title>Not Modified</title></head>"
    "<body><h1>304 Not Modified</h1></body>"
    "</html>";
const char bad_request[] =
    "<html>"
    "<head><title>Bad Request</title></head>"
    "<body><h1>400 Bad Request</h1></body>"
    "</html>";
const char unauthorized[] =
    "<html>"
    "<head><title>Unauthorized</title></head>"
    "<body><h1>401 Unauthorized</h1></body>"
    "</html>";
const char forbidden[] =
    "<html>"
    "<head><title>Forbidden</title></head>"
    "<body><h1>403 Forbidden</h1></body>"
    "</html>";
const char not_found[] =
    "<html>"
    "<head><title>Not Found</title></head>"
    "<body><h1>404 Not Found</h1></body>"
    "</html>";
const char length_required[] =
    "<html>"
    "<head><title>Length required</title></head>"
    "<body><h1>411 Length Required</h1></body>"
    "</html>";
const char internal_server_error[] =
    "<html>"
    "<head><title>Internal Server Error</title></head>"
    "<body><h1>500 Internal Server Error</h1></body>"
    "</html>";
const char not_implemented[] =
    "<html>"
    "<head><title>Not Implemented</title></head>"
    "<body><h1>501 Not Implemented</h1></body>"
    "</html>";
const char bad_gateway[] =
    "<html>"
    "<head><title>Bad Gateway</title></head>"
    "<body><h1>502 Bad Gateway</h1></body>"
    "</html>";
const char service_unavailable[] =
    "<html>"
    "<head><title>Service Unavailable</title></head>"
    "<body><h1>503 Service Unavailable</h1></body>"
    "</html>";
const char version_not_supported[] =
    "<html>"
    "<head><title>Version Not Supported</title></head>"
    "<body><h1>505 Version Not Supported</h1></body>"
    "</html>";

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

void Reply::stockReply(Reply::status_type status) {
    status_ = status;
    content_ = stock_replies::toArray(status);
    headers_.resize(2);
    headers_[0].name_ = "Content-Length";
    headers_[0].value_ = std::to_string(content_.size());
    headers_[1].name_ = "Content-Type";
    headers_[1].value_ = "text/html";

    returnToClient_ = true;
}

}  // namespace beauty
