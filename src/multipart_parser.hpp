#pragma once

#include <deque>
#include <string>
#include <tuple>
#include <vector>

#include "header.hpp"
#include "request.hpp"

namespace http {
namespace server {

struct Request;

// Parser for incoming requests.
class MultiPartParser {
   public:
    MultiPartParser(std::vector<char> &lastBuffer);

    // Reset to initial parser state.
    void reset();

    // Result of parse.
    enum result_type { done, bad, indeterminate };

    struct ContentPart {
        std::string filename_;
        std::vector<char>::iterator start_;
        std::vector<char>::iterator end_;
        bool headerOnly_ = false;
        bool foundStart_ = false;
        bool foundEnd_ = false;
    };

    // Return true if Content-Type is set to multipart.
    bool parseHeader(const Request &req);

    // Parse multipart content. The enum return value is done when all parts
    // has been parsed, bad if the data is invalid, indeterminate when more
    // data is required.
    // The caller must inspect readyParts to see if any parts has been
    // completed.
    result_type parse(const Request &req,
                      std::vector<char> &content,
                      std::deque<ContentPart> &parts);

    void flush(std::vector<char> &content, std::deque<ContentPart> &parts);

    const std::deque<ContentPart> &peakLastPart() const;

   private:
    // Handle the next character of input.
    result_type consume(std::vector<char>::iterator inputPtr, std::deque<ContentPart> &parts);

    // The current state of the parser.
    enum state {
        expecting_hyphen_1,
        expecting_hyphen_2,
        boundary_first,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3,
        part_data_start,
        part_data_cont,
        expecting_hyphen_3,
        boundary_next,
        boundary_close,
    } state_;

    std::vector<Header> headers_;
    std::vector<char> &lastBuffer_;
    std::deque<ContentPart> lastParts_;

    size_t contentCount_;
    size_t boundaryCount_;
    std::string boundaryStr_;
};

}  // namespace server
}  // namespace http
