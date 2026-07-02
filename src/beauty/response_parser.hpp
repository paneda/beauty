#pragma once

#include <limits>
#include <string>
#include <vector>

namespace beauty {

struct Response;

// Parser for incoming HTTP responses (client side).
class ResponseParser {
   public:
    ResponseParser();
    ~ResponseParser() = default;

    // Reset to initial parser state.
    void reset();

    // Result of parse.
    enum result_type {
        good_complete,        // A complete response (status line + headers + body) parsed
        good_part,            // Need more data to complete the response
        switching_protocols,  // 101 response - headers complete, no body (WebSocket upgrade)
        bad,                  // Malformed response
        version_not_supported,
        indeterminate
    };

    // Parse some data. Returns good_complete when a full response has been
    // parsed, bad on invalid data, good_part when more data is required and
    // switching_protocols when a 101 response was received.
    result_type parse(Response &res, std::vector<char> &content);

   private:
    // Handle the next character of input.
    result_type consume(Response &res, std::vector<char> &content, char input);

    void storeHeaderValueIfNeeded(Response &res);
    result_type checkResponseAfterAllHeaders(Response &res);

    // The current state of the parser.
    enum state {
        http_version_h,
        http_version_t_1,
        http_version_t_2,
        http_version_p,
        http_version_slash,
        http_version_major_start,
        http_version_major,
        http_version_minor_start,
        http_version_minor,
        status_code_start,
        status_code,
        status_message_start,
        status_message,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3,
        body,
    } state_;

    std::size_t contentLength_ = std::numeric_limits<size_t>::max();
    bool noBodyExpected_ = false;
};

}  // namespace beauty
