#include <string.h>
#include <algorithm>
#include <iostream>

#include "parse_common.hpp"
#include "multipart_parser.hpp"

namespace http {
namespace server {

MultiPartParser::MultiPartParser(std::vector<char> &lastBuffer)
    : state_(expecting_hyphen_1), lastBuffer_(lastBuffer) {}

void MultiPartParser::reset() {
    state_ = expecting_hyphen_1;
    boundaryStr_.clear();
    lastBuffer_.clear();
    lastParts_.clear();
}

bool MultiPartParser::parseHeader(const Request &req) {
    reset();
    std::string contentTypeVal = req.getHeaderValue("Content-Type");
    auto it = contentTypeVal.find("multipart");
    if (it == std::string::npos) {
        return false;
    }
    const std::string key = "boundary=";
    std::size_t foundStart = contentTypeVal.find(key);
    std::size_t foundEnd = contentTypeVal.find(";", foundStart + key.size());
    if (foundStart != std::string::npos && foundEnd > foundStart) {
        if (foundEnd == std::string::npos) {
            boundaryStr_ = contentTypeVal.substr(foundStart + key.size());
        } else {
            boundaryStr_ =
                contentTypeVal.substr(foundStart + key.size(), foundEnd - foundStart - key.size());
        }
    } else {
        return false;
    }
    return true;
}

MultiPartParser::result_type MultiPartParser::parse(const Request &req,
                                                    std::vector<char> &content,
                                                    std::deque<ContentPart> &parts) {
    result_type result;

    if (content.empty()) {
        return indeterminate;
    }

    parts.clear();
    auto begin = content.begin();
    auto end = content.end();
    while (begin != end) {
        result = consume(begin++, parts);
        if (result != indeterminate) {
            break;
        }
    }

    if (result == bad) {
        return result;
    }

    // if no filename_/start_/end_ was found in consume() it will not create
    // a part, so must then assume we're in the middle of content somewhere and
    // create a part
    if (result == indeterminate && parts.empty()) {
        parts.push_back(ContentPart());
    }

    // return the previous content (that have been adjusted below)
    parts.swap(lastParts_);
    content.swap(lastBuffer_);

    // make adjustments to the stored lastParts_ so they are correct when
    // swapped out in the next call
    bool containEndForPartToLeave = false;
    for (auto &lastPart : lastParts_) {
        // if start_ not found, assume buffer start
        if (!lastPart.foundStart_) {
            lastPart.start_ = lastBuffer_.begin();
        }

        // if end_ not found assume buffer end
        if (!lastPart.foundEnd_) {
            lastPart.end_ = lastBuffer_.end();
        } else {
            // lastPart.end_ is assigned +1 char after boundary so:
            // "boundary size" + 2*'-' + 2*clrf = 6
            // gives the correct position.
            lastPart.end_ = lastPart.end_ - boundaryStr_.size() - 6;
            if (lastPart.end_ < lastPart.start_) {
                // end was in the last part of the returned "parts".
                // "Luckily" (actually the whole reason why we're having
                // a lastPart/Buffer memory) we can adjust it here!
                ContentPart &partToLeave = parts.back();
                partToLeave.end_ = partToLeave.end_ - (lastPart.start_ - lastPart.end_);
                partToLeave.foundEnd_ = true;

                // this part needs removing as it was created
                // by consume() but only contained end_ for previous part
                containEndForPartToLeave = true;
            }
        }
    }

    if (containEndForPartToLeave) {
        lastParts_.pop_front();
    }

    return result;
}

void MultiPartParser::flush(std::vector<char> &content, std::deque<ContentPart> &parts) {
    parts.swap(lastParts_);
    content.swap(lastBuffer_);
    lastParts_.clear();
    lastBuffer_.clear();
}

const std::deque<MultiPartParser::ContentPart> &MultiPartParser::peakLastPart() const {
    return lastParts_;
}

MultiPartParser::result_type MultiPartParser::consume(std::vector<char>::iterator inputPtr,
                                                      std::deque<ContentPart> &parts) {
    char input = *inputPtr;
    switch (state_) {
        case expecting_hyphen_1:
            if (input != '-') {
                return bad;
            }
            state_ = expecting_hyphen_2;
            return indeterminate;
        case expecting_hyphen_2:
            if (input != '-') {
                return bad;
            }
            state_ = boundary_first;
            return indeterminate;
        case boundary_first:
            if (input == '\r') {
                state_ = expecting_newline_1;
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
            } else if (!headers_.empty() && (input == ' ' || input == '\t')) {
                state_ = header_lws;
            } else if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                headers_.push_back(Header());
                headers_.back().name_.reserve(20);
                headers_.back().value_.reserve(50);
                headers_.back().name_.push_back(input);
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
                headers_.back().value_.push_back(input);
            }
            return indeterminate;
        case header_name:
            if (input == ':') {
                state_ = space_before_header_value;
            } else if (!isChar(input) || isCtl(input) || isTsspecial(input)) {
                return bad;
            } else {
                headers_.back().name_.push_back(input);
            }
            return indeterminate;
        case space_before_header_value:
            if (input == ' ') {
                state_ = header_value;
            } else {
                return bad;
            }
            return indeterminate;
        case header_value: {
            if (input == '\r') {
                Header &h = headers_.back();

                if (strcasecmp(h.name_.c_str(), "Content-Disposition") == 0) {
                    const std::string key = "filename=\"";
                    std::size_t foundStart = h.value_.rfind(key);
                    std::size_t foundEnd = h.value_.find("\"", foundStart + key.size());
                    if (foundStart != std::string::npos && foundEnd != std::string::npos &&
                        foundEnd > foundStart) {
                        if (parts.empty()) {
                            parts.push_back(ContentPart());
                        }
                        parts.back().filename_ = h.value_.substr(
                            foundStart + key.size(), foundEnd - foundStart - key.size());
                        headers_.clear();
                    } else {
                        return bad;
                    }
                }
                state_ = expecting_newline_2;
            } else if (isCtl(input)) {
                return bad;
            } else {
                headers_.back().value_.push_back(input);
            }
            return indeterminate;
        }
        case expecting_newline_2:
            if (input == '\n') {
                state_ = header_line_start;
            } else {
                return bad;
            }
            return indeterminate;
        case expecting_newline_3: {
            if (input == '\n') {
                if (parts.empty()) {
                    parts.push_back(ContentPart());
                }
                parts.back().headerOnly_ = true;
                state_ = part_data_start;
            } else {
                return bad;
            }
            return indeterminate;
        }
        case part_data_start:
            if (parts.empty()) {
                parts.push_back(ContentPart());
            }
            parts.back().headerOnly_ = false;
            parts.back().start_ = inputPtr;
            parts.back().foundStart_ = true;
            state_ = part_data_cont;
            return indeterminate;
        case part_data_cont:
            if (input == '-') {
                state_ = expecting_hyphen_3;
            }
            return indeterminate;
        case expecting_hyphen_3:
            if (input == '-') {
                state_ = boundary_next;
                boundaryCount_ = 0;
            } else {
                state_ = part_data_cont;
            }
            return indeterminate;
        case boundary_next:
            if (input == boundaryStr_[boundaryCount_++]) {
                if (boundaryCount_ == boundaryStr_.size()) {
                    state_ = boundary_close;
                }
            } else {
                boundaryCount_ = 0;
                state_ = part_data_cont;
            }
            return indeterminate;
        case boundary_close: {
            if (parts.empty()) {
                parts.push_back(ContentPart());
            }
            parts.back().end_ = inputPtr;
            parts.back().foundEnd_ = true;

            if (input == '-') {
                return done;
            } else if (input == '\r') {
                parts.push_back(ContentPart());
                state_ = expecting_newline_1;
            } else {
                return bad;
            }
            return indeterminate;
        }
        default:
            return bad;
    }
}

}  // namespace server
}  // namespace http
