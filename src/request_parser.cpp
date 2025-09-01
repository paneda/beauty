#include <algorithm>
#include <strings.h>

#include "beauty/parse_common.hpp"
#include "beauty/request.hpp"
#include "beauty/request_parser.hpp"

namespace beauty {

RequestParser::RequestParser() : state_(method_start) {}

void RequestParser::reset() {
    state_ = method_start;
}

RequestParser::result_type RequestParser::parse(Request &req, std::vector<char> &content) {
    auto begin = content.begin();
    auto end = content.end();
    while (begin != end) {
        result_type result = consume(req, content, *begin++);
        if (result != indeterminate) {
            return result;
        }
    }
    return good_part;
}

RequestParser::result_type RequestParser::consume(Request &req,
                                                  std::vector<char> &content,
                                                  char input) {
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
                return good_complete;
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
                if (req.method_ == "POST" || req.method_ == "PUT" || req.method_ == "PATCH") {
                    Header &h = req.headers_.back();

                    if (strcasecmp(h.name_.c_str(), "Content-Length") == 0) {
                        contentLength_ = atoi(h.value_.c_str());
                        req.contentLength_ = contentLength_;
                        contentLength_ = std::min(content.capacity(), contentLength_);
                    } else if (strcasecmp(h.name_.c_str(), "Transfer-Encoding") == 0) {
                        if (strcasecmp(h.value_.c_str(), "chunked") == 0) {
                            if (req.httpVersionMajor_ < 1 ||
                                (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ < 1)) {
                                // chunked encoding not supported in HTTP/1.0 or earlier
                                return bad;
                            }
                            return not_implemented;
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
                req.keepAlive_ = false;
                if (req.httpVersionMajor_ > 1 ||
                    (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ == 1)) {
                    req.keepAlive_ = true;
                }
            }

            // start filling up body data
            content.clear();
            if (contentLength_ == 0) {
                if (input == '\n') {
                    return good_complete;
                } else {
                    return bad;
                }
            } else {
                req.noInitialBodyBytesReceived_ = 0;
                state_ = post;
            }
            return indeterminate;
        }
        case post:
            --contentLength_;
            req.noInitialBodyBytesReceived_++;
            content.push_back(input);
            if (contentLength_ == 0) {
                return good_complete;
            }
            return indeterminate;
        default:
            return bad;
    }
}

}  // namespace beauty
