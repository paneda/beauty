#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "beauty/ws_parser.hpp"

namespace beauty {

namespace {
const uint8_t FinMask = 0x80;
const uint8_t OpMask = 0x0f;
const uint8_t MaskMask = 0x80;
const uint8_t LengthMask = 0x7f;
}

WsParser::WsParser(WsReceive &wsRecv) : wsRecv_(wsRecv) {}

WsParser::result_type WsParser::parse() {
    result_type result = indeterminate;

    if (wsRecv_.content_.empty()) {
        return result;
    }

    auto begin = wsRecv_.content_.begin();
    auto end = wsRecv_.content_.end();
    while (begin != end) {
        result = consume(begin++);
        if (result != indeterminate) {
            break;
        }
    }

    wsRecv_.content_.resize(wsRecv_.outCounter_);
    return result;
}

WsParser::result_type WsParser::consume(std::vector<char>::iterator inPtr) {
    uint8_t input = *inPtr;
    switch (state_) {
        case s_start:
            isFin_ = input & FinMask;
            opCode_ = (OpCode)(input & OpMask);

            // Reject fragmentation - not currently supported
            if (opCode_ == Continuation) {
                return fragmentation_error;
            }
            if (!isFin_ && (opCode_ == TextData || opCode_ == BinData)) {
                // Non-final data frame - would start fragmentation
                return fragmentation_error;
            }

            maskCounter_ = 0;
            wsRecv_.payLoadCounter_ = 0;
            wsRecv_.isFinal_ = false;
            state_ = s_mask_and_len;
            return indeterminate;
        case s_mask_and_len:
            hasMask_ = input & MaskMask;
            payloadLen_ = input & LengthMask;
            if (payloadLen_ == 126) {
                extLenBytes_ = 2;
                payloadLen_ = 0;
                state_ = s_ext_length;
            } else if (payloadLen_ == 127) {
                extLenBytes_ = 8;
                payloadLen_ = 0;
                state_ = s_ext_length;
            } else if (hasMask_) {
                state_ = s_mask_1;
            } else {
                mask_[0] = 0xff;
                mask_[1] = 0xff;
                mask_[2] = 0xff;
                mask_[3] = 0xff;
                state_ = getOpCodeState();
                if (payloadLen_ == 0) {
                    return handleZeroLengthPayload();
                }
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
                    state_ = getOpCodeState();
                    if (payloadLen_ == 0) {
                        return handleZeroLengthPayload();
                    }
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
            state_ = getOpCodeState();
            if (payloadLen_ == 0) {
                return handleZeroLengthPayload();
            }
            return indeterminate;
        case s_payload:
            wsRecv_.content_[wsRecv_.outCounter_++] = input ^ mask_[maskCounter_++ % 4];
            if (++wsRecv_.payLoadCounter_ >= payloadLen_) {
                state_ = s_start;
                wsRecv_.isFinal_ = true;
                wsRecv_.payLoadCounter_ = 0;
                return data_frame;
            }
            return indeterminate;
        case s_close:
            wsRecv_.content_[wsRecv_.outCounter_++] = input ^ mask_[maskCounter_++ % 4];
            if (++wsRecv_.payLoadCounter_ >= payloadLen_) {
                state_ = s_start;
                wsRecv_.isFinal_ = true;
                wsRecv_.payLoadCounter_ = 0;
                return close_frame;
            }
            return indeterminate;
        case s_ping:
            wsRecv_.content_[wsRecv_.outCounter_++] = input ^ mask_[maskCounter_++ % 4];
            if (++wsRecv_.payLoadCounter_ >= payloadLen_) {
                state_ = s_start;
                wsRecv_.isFinal_ = true;
                wsRecv_.payLoadCounter_ = 0;
                return ping_frame;
            }
            return indeterminate;
        case s_pong:
            wsRecv_.content_[wsRecv_.outCounter_++] = input ^ mask_[maskCounter_++ % 4];
            if (++wsRecv_.payLoadCounter_ >= payloadLen_) {
                state_ = s_start;
                wsRecv_.isFinal_ = true;
                wsRecv_.payLoadCounter_ = 0;
                return pong_frame;
            }
            return indeterminate;
        default:
            // should never get here
            assert(false);
            return indeterminate;
    }
}

WsParser::State WsParser::getOpCodeState() {
    switch (opCode_) {
        case Continuation:
            // Fragmentation not supported in embedded environment
            // This should never be reached as we detect fragmentation earlier
            assert(false);
            return s_start;
        case TextData:
        case BinData:
            return s_payload;
        case Close:
            return s_close;
        case Ping:
            return s_ping;
        case Pong:
            return s_pong;
        default:
            assert(false);
            return s_start;
    }
}

WsParser::result_type WsParser::getResultType() {
    switch (opCode_) {
        case TextData:
        case BinData:
            return data_frame;
        case Close:
            return close_frame;
        case Ping:
            return ping_frame;
        case Pong:
            return pong_frame;
        default:
            // For continuation or unknown, default to data_frame
            return data_frame;
    }
}

WsParser::result_type WsParser::handleZeroLengthPayload() {
    state_ = s_start;
    wsRecv_.isFinal_ = true;
    wsRecv_.payLoadCounter_ = 0;
    return getResultType();
}

}  // namespace beauty
