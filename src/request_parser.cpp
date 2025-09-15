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
    size_t totalContentLength = content.size();

    while (begin != end) {
        result_type result = consume(req, content, *begin++);
        if (result != indeterminate) {
            // Check for 100-continue protocol violation after parsing completes
            if (result == good_headers_expect_continue && std::distance(begin, end) > 0) {
                // Client sent Expect: 100-continue but included body data without waiting
                return expect_continue_with_body;
            }
            return result;
        }
    }

    if (req.contentLength_ == std::numeric_limits<size_t>::max()) {
        // As we may not have received the Content-Length header for HTTP/1.0
        // requests, we decide good_part or good_complete on whether the
        // content fits in the buffer.
        if (totalContentLength < content.capacity()) {
            req.contentLength_ = content.size();
            return good_complete;
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
                if (req.httpVersionMajor_ > 1 ||
                    (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ > 1)) {
                    return version_not_supported;
                }
                // Set default keep-alive based on HTTP version.
                // Presence of a Connection header may override this later.
                req.keepAlive_ = (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ > 0);
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
                storeHeaderValueIfNeeded(req);
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
            result_type res = checkRequestAfterAllHeaders(req);
            if (res != indeterminate) {
                return res;
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

void RequestParser::storeHeaderValueIfNeeded(Request &req) {
    Header &h = req.headers_.back();

    if (strcasecmp(h.name_.c_str(), "Content-Length") == 0) {
        size_t actualContentLength = atoi(h.value_.c_str());
        req.contentLength_ = actualContentLength;
        contentLength_ = actualContentLength;
    } else if (strcasecmp(h.name_.c_str(), "Transfer-Encoding") == 0) {
        if (strcasecmp(h.value_.c_str(), "chunked") == 0) {
            req.isChunked_ = true;
        }
    } else if (strcasecmp(h.name_.c_str(), "Expect") == 0) {
        if (strcasecmp(h.value_.c_str(), "100-continue") == 0) {
            req.expectContinue_ = true;
        }
    } else if (strcasecmp(h.name_.c_str(), "Connection") == 0) {
        if (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ < 1) {
            // HTTP/1.0: Keep-Alive must be explicitly specified
            if (strcasecmp(h.value_.c_str(), "Keep-Alive") == 0) {
                req.keepAlive_ = true;
            }
        } else {
            // HTTP/1.1+: Keep-Alive is default unless "close" is specified
            if (strcasecmp(h.value_.c_str(), "close") == 0) {
                req.keepAlive_ = false;
            }
        }
    }
}

RequestParser::result_type RequestParser::checkRequestAfterAllHeaders(Request &req) {
    if ((req.method_ == "GET" || req.method_ == "DELETE" || req.method_ == "HEAD" ||
         req.method_ == "TRACE" || req.method_ == "OPTIONS")) {
        if (req.expectContinue_ || req.isChunked_ ||
            req.contentLength_ != std::numeric_limits<size_t>::max()) {
            // header are invalid for GET/HEAD/DELETE/TRACE/OPTIONS
            return bad;
        }
        contentLength_ = 0;
    } else if (req.method_ == "POST" || req.method_ == "PUT" || req.method_ == "PATCH") {
        if (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ > 0) {
            if (req.isChunked_) {
                // setting Transfer-Encoding: chunked and Content-Length is invalid
                return req.contentLength_ == std::numeric_limits<size_t>::max()
                           ? missing_content_length
                           : bad;
            } else if (req.contentLength_ == std::numeric_limits<size_t>::max()) {
                return missing_content_length;
            }

            if (req.expectContinue_) {
                return good_headers_expect_continue;
            }
        }
    }
    return indeterminate;
}

}  // namespace beauty
