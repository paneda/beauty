#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "ws_parser.hpp"

namespace beauty {
// #ifdef assert
// # define assertFalse(msg) assert(0 && msg)
// #else
// # define assertFalse(msg)
// #endif

namespace {
const uint8_t FinMask = 0x80;
const uint8_t OpMask = 0x0f;
const uint8_t MaskMask = 0x80;
const uint8_t LengthMask = 0x7f;
}

WsParser::WsParser(WsReceive &wsRecv) : wsRecv_(wsRecv) {}

WsParser::result_type WsParser::parse() {
    result_type result;

    if (wsRecv_.content_.empty()) {
        return indeterminate;
    }

    auto begin = wsRecv_.content_.begin();
    auto end = wsRecv_.content_.end();
    while (begin != end) {
        result = consume(begin++);
        if (result != indeterminate) {
            wsRecv_.content_.resize(payloadLen_);
            return result;
        }
    }

    wsRecv_.content_.resize(payloadLen_);
    return indeterminate;
}

WsParser::result_type WsParser::consume(std::vector<char>::iterator inPtr) {
    uint8_t input = *inPtr;
    switch (state_) {
        case s_start:
            isFin_ = input & FinMask;
            opCodeDF_ = (OpCodeDataFrame)(input & OpMask);
            payloadLen_ = 0;
            state_ = s_mask_and_len;
            return indeterminate;
        case s_mask_and_len:
            hasMask_ = input & MaskMask;
            payloadLen_ = input & LengthMask;
            if (payloadLen_ == 126) {
                extLenBytes_ = 2;
                state_ = s_ext_length;
            } else if (payloadLen_ == 127) {
                extLenBytes_ = 8;
                state_ = s_ext_length;
            } else if (hasMask_) {
                state_ = s_mask_1;
            } else {
                mask_[0] = 0xff;
                mask_[1] = 0xff;
                mask_[2] = 0xff;
                mask_[3] = 0xff;
                state_ = s_payload;
            }
            return indeterminate;
        case s_ext_length:
            payloadLen_ <<= 8;
            payloadLen_ |= input;
            extLenBytes_--;
            if (extLenBytes_ == 0) {
                if (hasMask_) {
                    state_ = s_mask_1;
                } else {
                    state_ = s_payload;
                }
            }
            return indeterminate;
        case s_mask_1:
            mask_[0] = input;
            state_ = s_mask_2;
            return indeterminate;
        case s_mask_2:
            mask_[1] = input;
            state_ = s_mask_3;
            return indeterminate;
        case s_mask_3:
            mask_[2] = input;
            state_ = s_mask_4;
            return indeterminate;
        case s_mask_4:
            mask_[3] = input;
            state_ = s_payload;
            return indeterminate;
        case s_payload:
            wsRecv_.content_[wsRecv_.outCounter_] = input ^ mask_[wsRecv_.outCounter_ % 4];
            if (++wsRecv_.outCounter_ >= payloadLen_) {
                return done;
            }
            return indeterminate;
        default:
            return bad;
    }
}

}  // namespace beauty
