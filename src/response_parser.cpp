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
    isChunked_ = false;
    interimResponse_ = false;
    chunkSizeDigitSeen_ = false;
    chunkRemaining_ = 0;
}

void ResponseParser::setMaxBodySize(size_t maxBodySize) {
    maxBodySize_ = maxBodySize;
}

ResponseParser::result_type ResponseParser::finish(Response &res) {
    // The peer closed the connection. A response with neither Content-Length
    // nor chunked encoding is delimited by the close itself, so whatever body
    // bytes we accumulated form the complete body.
    if (state_ == body && contentLength_ == std::numeric_limits<size_t>::max() && !isChunked_) {
        res.keepAlive_ = false;
        return good_complete;
    }
    // Anything else means the connection closed mid-response.
    (void)res;
    return bad;
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
            if (interimResponse_) {
                // A 1xx interim response (other than 101) is not the final
                // response. Discard it and continue parsing the response that
                // follows on the same connection.
                res.reset();
                reset();
                return indeterminate;
            }
            // start filling up body data
            res.body_.clear();
            if (noBodyExpected_) {
                return good_complete;
            }
            if (isChunked_) {
                chunkRemaining_ = 0;
                state_ = chunk_size;
                return indeterminate;
            }
            if (contentLength_ == 0) {
                return good_complete;
            }
            state_ = body;
            return indeterminate;
        }
        case body:
            if (res.body_.size() >= maxBodySize_) {
                return too_large;
            }
            res.body_.push_back(input);
            if (contentLength_ != std::numeric_limits<size_t>::max()) {
                if (res.body_.size() >= contentLength_) {
                    return good_complete;
                }
            }
            return indeterminate;
        case chunk_size:
            if (isHexDigit(input)) {
                chunkSizeDigitSeen_ = true;
                chunkRemaining_ = chunkRemaining_ * 16 + hexValue(input);
            } else if (input == ';') {
                if (!chunkSizeDigitSeen_) {
                    return bad;
                }
                state_ = chunk_extension;
            } else if (input == '\r') {
                if (!chunkSizeDigitSeen_) {
                    return bad;
                }
                state_ = chunk_size_newline;
            } else {
                return bad;
            }
            return indeterminate;
        case chunk_extension:
            // Chunk extensions are ignored.
            if (input == '\r') {
                state_ = chunk_size_newline;
            }
            return indeterminate;
        case chunk_size_newline:
            if (input != '\n') {
                return bad;
            }
            if (chunkRemaining_ == 0) {
                // Last chunk - trailers (if any) follow.
                state_ = chunk_trailer_start;
            } else {
                state_ = chunk_data;
            }
            return indeterminate;
        case chunk_data:
            if (res.body_.size() >= maxBodySize_) {
                return too_large;
            }
            res.body_.push_back(input);
            if (--chunkRemaining_ == 0) {
                state_ = chunk_data_cr;
            }
            return indeterminate;
        case chunk_data_cr:
            if (input != '\r') {
                return bad;
            }
            state_ = chunk_data_newline;
            return indeterminate;
        case chunk_data_newline:
            if (input != '\n') {
                return bad;
            }
            chunkRemaining_ = 0;
            chunkSizeDigitSeen_ = false;
            state_ = chunk_size;
            return indeterminate;
        case chunk_trailer_start:
            if (input == '\r') {
                state_ = chunk_trailer_end_newline;
            } else {
                state_ = chunk_trailer_line;
            }
            return indeterminate;
        case chunk_trailer_line:
            if (input == '\r') {
                state_ = chunk_trailer_line_newline;
            }
            return indeterminate;
        case chunk_trailer_line_newline:
            if (input != '\n') {
                return bad;
            }
            state_ = chunk_trailer_start;
            return indeterminate;
        case chunk_trailer_end_newline:
            if (input != '\n') {
                return bad;
            }
            return good_complete;
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
    if (res.statusCode_ == 101) {
        return switching_protocols;
    }

    // 1xx informational responses (other than 101) are interim: the real
    // response follows on the same connection and must be parsed next.
    if (res.statusCode_ >= 100 && res.statusCode_ < 200) {
        interimResponse_ = true;
        return indeterminate;
    }

    // 204 No Content and 304 Not Modified never carry a body.
    if (res.statusCode_ == 204 || res.statusCode_ == 304) {
        noBodyExpected_ = true;
        return indeterminate;
    }

    // A chunked response is self-delimiting regardless of Content-Length.
    if (res.isChunked_) {
        isChunked_ = true;
    }

    return indeterminate;
}

}  // namespace beauty
