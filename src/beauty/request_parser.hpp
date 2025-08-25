#pragma once

#include <vector>

namespace beauty {

struct Request;

// Parser for incoming requests.
class RequestParser {
   public:
    RequestParser();

    // Reset to initial parser state.
    void reset();

    // Result of parse.
    enum result_type { good_complete, good_part, bad, indeterminate };

    // Parse some data. The enum return value is good when a complete request
    // has been parsed, bad if the data is invalid, good_part when more
    // data is required.
    result_type parse(Request &req, std::vector<char> &content);

   private:
    // Handle the next character of input.
    result_type consume(Request &req, std::vector<char> &content, char input);

    // The current state of the parser.
    enum state {
        method_start,
        method,
        uri_start,
        uri,
        http_version_h,
        http_version_t_1,
        http_version_t_2,
        http_version_p,
        http_version_slash,
        http_version_major_start,
        http_version_major,
        http_version_minor_start,
        http_version_minor,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3,
        post,
    } state_;

    std::size_t contentLength_ = 0;
};

}  // namespace beauty
