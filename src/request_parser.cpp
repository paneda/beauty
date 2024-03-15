#include "request_parser.hpp"

#include <string.h>

#include <algorithm>

#include "request.hpp"

namespace http {
namespace server {

RequestParser::RequestParser() : state_(method_start) {}

void RequestParser::reset() {
    state_ = method_start;
}

RequestParser::result_type RequestParser::consume(Request &req, char input) {
    switch (state_) {
        case method_start:
            if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                state_ = method;
                req.method_.push_back(input);
            }
            return indeterminate;
        case method:
            if (input == ' ') {
                state_ = uri_start;
            } else if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                req.method_.push_back(input);
            }
            return indeterminate;
        case uri_start:
            if (isCtl(input)) {
                return bad;
            } else {
                state_ = uri;
                req.uri_.push_back(input);
            }
            return indeterminate;
        case uri:
            if (input == ' ') {
                state_ = http_version_h;
            } else if (input == '\r') {
                req.httpVersionMajor_ = 0;
                req.httpVersionMinor_ = 9;
                return good;
            } else if (isCtl(input)) {
                return bad;
            } else {
                req.uri_.push_back(input);
            }
            return indeterminate;
        case http_version_h:
            if (input == 'H') {
                state_ = http_version_t_1;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_t_1:
            if (input == 'T') {
                state_ = http_version_t_2;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_t_2:
            if (input == 'T') {
                state_ = http_version_p;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_p:
            if (input == 'P') {
                state_ = http_version_slash;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_slash:
            if (input == '/') {
                req.httpVersionMajor_ = 0;
                req.httpVersionMinor_ = 0;
                state_ = http_version_major_start;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_major_start:
            if (isDigit(input)) {
                req.httpVersionMajor_ = req.httpVersionMajor_ * 10 + input - '0';
                state_ = http_version_major;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_major:
            if (input == '.') {
                state_ = http_version_minor_start;
            } else if (isDigit(input)) {
                req.httpVersionMajor_ = req.httpVersionMajor_ * 10 + input - '0';
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_minor_start:
            if (isDigit(input)) {
                req.httpVersionMinor_ = req.httpVersionMinor_ * 10 + input - '0';
                state_ = http_version_minor;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_minor:
            if (input == '\r') {
                state_ = expecting_newline_1;
            } else if (isDigit(input)) {
                req.httpVersionMinor_ = req.httpVersionMinor_ * 10 + input - '0';
            } else {
                return bad;
            }
            return indeterminate;
        case expecting_newline_1:
            if (input == '\n') {
                state_ = header_line_start;
            } else {
                return bad;
            }
            return indeterminate;
        case header_line_start:
            if (input == '\r') {
                state_ = expecting_newline_3;
            } else if (!req.headers_.empty() && (input == ' ' || input == '\t')) {
                state_ = header_lws;
            } else if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                req.headers_.push_back(Header());
                req.headers_.back().name_.reserve(16);
                req.headers_.back().value_.reserve(16);
                req.headers_.back().name_.push_back(input);
                state_ = header_name;
            }
            return indeterminate;
        case header_lws:
            if (input == '\r') {
                state_ = expecting_newline_2;
            } else if (input == ' ' || input == '\t') {
                // skip
            } else if (isCtl(input)) {
                return bad;
            } else {
                state_ = header_value;
                req.headers_.back().value_.push_back(input);
            }
            return indeterminate;
        case header_name:
            if (input == ':') {
                state_ = space_before_header_value;
            } else if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                req.headers_.back().name_.push_back(input);
            }
            return indeterminate;
        case space_before_header_value:
            if (input == ' ') {
                state_ = header_value;
            } else {
                return bad;
            }
            return indeterminate;
        case header_value:
            if (input == '\r') {
                if (req.method_ == "POST" || req.method_ == "PUT") {
                    Header &h = req.headers_.back();

                    if (strcasecmp(h.name_.c_str(), "Content-Length") == 0) {
                        bodySize_ = atoi(h.value_.c_str());
                        req.body_.reserve(bodySize_);
                    } else if (strcasecmp(h.name_.c_str(), "Transfer-Encoding") == 0) {
                        if (strcasecmp(h.value_.c_str(), "chunked") == 0) {
                            chunked_ = true;
                        }
                    }
                }
                state_ = expecting_newline_2;
            } else if (isCtl(input)) {
                return bad;
            } else {
                req.headers_.back().value_.push_back(input);
            }
            return indeterminate;
        case expecting_newline_2:
            if (input == '\n') {
                state_ = header_line_start;
            } else {
                return bad;
            }
            return indeterminate;
        case expecting_newline_3: {
            std::vector<Header>::iterator it =
                std::find_if(req.headers_.begin(), req.headers_.end(), [](const Header &item) {
                    return strcasecmp(item.name_.c_str(), "Connection") == 0;
                });

            if (it != req.headers_.end()) {
                if (strcasecmp(it->value_.c_str(), "Keep-Alive") == 0) {
                    req.keepAlive_ = true;
                } else {
                    req.keepAlive_ = false;
                }
            } else {
                if (req.httpVersionMajor_ > 1 ||
                    (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ == 1)) {
                    req.keepAlive_ = true;
                }
            }

            if (chunked_) {
                state_ = chunk_size;
            } else if (bodySize_ == 0) {
                if (input == '\n') {
                    return good;
                } else {
                    return bad;
                }
            } else {
                state_ = post;
            }
            return indeterminate;
        }
        case post:
            --bodySize_;
            req.body_.push_back(input);
            if (bodySize_ == 0) {
                return good;
            }
            return indeterminate;
        case chunk_size:
            if (isalnum(input)) {
                chunkSizeStr_.push_back(input);
            } else if (input == ';') {
                state_ = chunk_extension_name;
            } else if (input == '\r') {
                state_ = chunk_size_newline;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_extension_name:
            if (isalnum(input) || input == ' ') {
                // skip
            } else if (input == '=') {
                state_ = chunk_extension_value;
            } else if (input == '\r') {
                state_ = chunk_size_newline;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_extension_value:
            if (isalnum(input) || input == ' ') {
                // skip
            } else if (input == '\r') {
                state_ = chunk_size_newline;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_size_newline:
            if (input == '\n') {
                chunkSize_ = strtol(chunkSizeStr_.c_str(), NULL, 16);
                chunkSizeStr_.clear();
                req.body_.reserve(req.body_.size() + chunkSize_);

                if (chunkSize_ == 0) {
                    state_ = chunk_size_newline_2;
                } else {
                    state_ = chunk_data;
                }
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_size_newline_2:
            if (input == '\r') {
                state_ = chunk_size_newline_3;
            } else if (isalpha(input)) {
                state_ = chunk_trailer_name;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_size_newline_3:
            if (input == '\n') {
                return good;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_trailer_name:
            if (isalnum(input)) {
                // skip
            } else if (input == ':') {
                state_ = chunk_trailer_value;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_trailer_value:
            if (isalnum(input) || input == ' ') {
                // skip
            } else if (input == '\r') {
                state_ = chunk_size_newline;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_data:
            req.body_.push_back(input);
            if (--chunkSize_ == 0) {
                state_ = chunk_data_newline_1;
            }
            return indeterminate;
        case chunk_data_newline_1:
            if (input == '\r') {
                state_ = chunk_data_newline_2;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_data_newline_2:
            if (input == '\n') {
                state_ = chunk_size;
            } else {
                return bad;
            }
            return indeterminate;
        default:
            return bad;
    }
}

bool RequestParser::isChar(int c) {
    return c >= 0 && c <= 127;
}

bool RequestParser::isCtl(int c) {
    return (c >= 0 && c <= 31) || (c == 127);
}

bool RequestParser::isTsspecial(int c) {
    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
    }
}

bool RequestParser::isDigit(int c) {
    return c >= '0' && c <= '9';
}

}  // namespace server
}  // namespace http
