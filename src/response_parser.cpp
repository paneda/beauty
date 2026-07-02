#include <strings.h>

#include <algorithm>

#include "beauty/parse_common.hpp"
#include "beauty/response.hpp"
#include "beauty/response_parser.hpp"

namespace beauty {

ResponseParser::ResponseParser() : state_(http_version_h) {}

void ResponseParser::reset() {
    state_ = http_version_h;
    contentLength_ = std::numeric_limits<size_t>::max();
    noBodyExpected_ = false;
}

ResponseParser::result_type ResponseParser::parse(Response &res, std::vector<char> &content) {
    auto begin = content.begin();
    auto end = content.end();

    result_type result = good_part;
    while (begin != end) {
        result = consume(res, content, *begin++);
        if (result != indeterminate) {
            break;
        }
        result = good_part;
    }

    // Remove the bytes we consumed from the front of the receive buffer. Any
    // leftover bytes (e.g. the start of the next response or a WebSocket frame
    // that arrived in the same segment) remain for the caller to process.
    content.erase(content.begin(), begin);

    return result;
}

ResponseParser::result_type ResponseParser::consume(Response &res,
                                                    std::vector<char> &content,
                                                    char input) {
    (void)content;
    switch (state_) {
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
                res.httpVersionMajor_ = 0;
                res.httpVersionMinor_ = 0;
                state_ = http_version_major_start;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_major_start:
            if (isDigit(input)) {
                res.httpVersionMajor_ = res.httpVersionMajor_ * 10 + input - '0';
                state_ = http_version_major;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_major:
            if (input == '.') {
                state_ = http_version_minor_start;
            } else if (isDigit(input)) {
                res.httpVersionMajor_ = res.httpVersionMajor_ * 10 + input - '0';
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_minor_start:
            if (isDigit(input)) {
                res.httpVersionMinor_ = res.httpVersionMinor_ * 10 + input - '0';
                state_ = http_version_minor;
            } else {
                return bad;
            }
            return indeterminate;
        case http_version_minor:
            if (input == ' ') {
                state_ = status_code_start;
                if (res.httpVersionMajor_ > 1 ||
                    (res.httpVersionMajor_ == 1 && res.httpVersionMinor_ > 1)) {
                    return version_not_supported;
                }
                // Default keep-alive based on HTTP version. A Connection header
                // may override this later.
                res.keepAlive_ = (res.httpVersionMajor_ == 1 && res.httpVersionMinor_ > 0);
            } else if (isDigit(input)) {
                res.httpVersionMinor_ = res.httpVersionMinor_ * 10 + input - '0';
            } else {
                return bad;
            }
            return indeterminate;
        case status_code_start:
            if (isDigit(input)) {
                res.statusCode_ = res.statusCode_ * 10 + input - '0';
                state_ = status_code;
            } else {
                return bad;
            }
            return indeterminate;
        case status_code:
            if (input == ' ') {
                state_ = status_message_start;
            } else if (isDigit(input)) {
                res.statusCode_ = res.statusCode_ * 10 + input - '0';
            } else if (input == '\r') {
                // Some servers omit the reason phrase.
                state_ = expecting_newline_1;
            } else {
                return bad;
            }
            return indeterminate;
        case status_message_start:
            if (input == '\r') {
                state_ = expecting_newline_1;
            } else if (isCtl(input)) {
                return bad;
            } else {
                res.statusMessage_.push_back(input);
                state_ = status_message;
            }
            return indeterminate;
        case status_message:
            if (input == '\r') {
                state_ = expecting_newline_1;
            } else if (isCtl(input)) {
                return bad;
            } else {
                res.statusMessage_.push_back(input);
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
            } else if (!res.headers_.empty() && (input == ' ' || input == '\t')) {
                state_ = header_lws;
            } else if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                res.headers_.push_back(Header());
                res.headers_.back().name_.reserve(16);
                res.headers_.back().value_.reserve(16);
                res.headers_.back().name_.push_back(input);
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
                res.headers_.back().value_.push_back(input);
            }
            return indeterminate;
        case header_name:
            if (input == ':') {
                state_ = space_before_header_value;
            } else if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                res.headers_.back().name_.push_back(input);
            }
            return indeterminate;
        case space_before_header_value:
            if (input == ' ') {
                state_ = header_value;
            } else if (input == '\r') {
                // Header with empty value.
                state_ = expecting_newline_2;
            } else {
                return bad;
            }
            return indeterminate;
        case header_value:
            if (input == '\r') {
                storeHeaderValueIfNeeded(res);
                state_ = expecting_newline_2;
            } else if (isCtl(input)) {
                return bad;
            } else {
                res.headers_.back().value_.push_back(input);
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
            if (input != '\n') {
                return bad;
            }
            result_type res2 = checkResponseAfterAllHeaders(res);
            if (res2 != indeterminate) {
                return res2;
            }
            // start filling up body data
            res.body_.clear();
            if (contentLength_ == 0) {
                return good_complete;
            }
            state_ = body;
            return indeterminate;
        }
        case body:
            res.body_.push_back(input);
            if (contentLength_ != std::numeric_limits<size_t>::max()) {
                if (res.body_.size() >= contentLength_) {
                    return good_complete;
                }
            }
            return indeterminate;
        default:
            return bad;
    }
}

void ResponseParser::storeHeaderValueIfNeeded(Response &res) {
    Header &h = res.headers_.back();

    if (strcasecmp(h.name_.c_str(), "Content-Length") == 0) {
        size_t actualContentLength = 0;
        if (parseUint(h.value_.c_str(), actualContentLength)) {
            res.contentLength_ = actualContentLength;
            contentLength_ = actualContentLength;
        }
    } else if (strcasecmp(h.name_.c_str(), "Transfer-Encoding") == 0) {
        if (strcasecmp(h.value_.c_str(), "chunked") == 0) {
            res.isChunked_ = true;
        }
    } else if (strcasecmp(h.name_.c_str(), "Connection") == 0) {
        if (strcasecmp(h.value_.c_str(), "close") == 0) {
            res.keepAlive_ = false;
        } else if (strcasecmp(h.value_.c_str(), "Keep-Alive") == 0) {
            res.keepAlive_ = true;
        }
    }
}

ResponseParser::result_type ResponseParser::checkResponseAfterAllHeaders(Response &res) {
    // 1xx informational, 204 No Content and 304 Not Modified never carry a body.
    if ((res.statusCode_ >= 100 && res.statusCode_ < 200) || res.statusCode_ == 204 ||
        res.statusCode_ == 304) {
        contentLength_ = 0;
        noBodyExpected_ = true;
    }

    if (res.statusCode_ == 101) {
        return switching_protocols;
    }

    return indeterminate;
}

}  // namespace beauty
