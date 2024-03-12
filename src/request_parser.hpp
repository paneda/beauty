#pragma once

#include <string>
#include <tuple>

namespace http {
namespace server {

struct Request;

// Parser for incoming requests.
class RequestParser {
   public:
    RequestParser();

    // Reset to initial parser state.
    void reset();

    // Result of parse.
    enum result_type { good, bad, indeterminate };

    // Parse some data. The enum return value is good when a complete request
    // has been parsed, bad if the data is invalid, indeterminate when more
    // data is required. The InputIterator return value indicates how much of
    // the input has been consumed.
    template <typename InputIterator>
    std::tuple<result_type, InputIterator> parse(Request &req,
                                                 InputIterator begin,
                                                 InputIterator end) {
        while (begin != end) {
            result_type result = consume(req, *begin++);
            if (result == good || result == bad)
                return std::make_tuple(result, begin);
        }
        return std::make_tuple(indeterminate, begin);
    }

   private:
    // Handle the next character of input.
    result_type consume(Request &req, char input);

    // Check if a byte is an HTTP character.
    static bool isChar(int c);

    // Check if a byte is an HTTP control character.
    static bool isCtl(int c);

    // Check if a byte is defined as an HTTP tspecial character.
    static bool isTsspecial(int c);

    // Check if a byte is a digit.
    static bool isDigit(int c);

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
        chunk_size,
        chunk_extension_name,
        chunk_extension_value,
        chunk_size_newline,
        chunk_size_newline_2,
        chunk_size_newline_3,
        chunk_trailer_name,
        chunk_trailer_value,
        chunk_data_newline_1,
        chunk_data_newline_2,
        chunk_data
    } state_;

    std::size_t bodySize_ = 0;
    std::string chunkSizeStr_;
    std::size_t chunkSize_ = 0;
    bool chunked_ = false;
};

}  // namespace server
}  // namespace http
