#pragma once

#include <array>
#include <vector>
#include <stdint.h>

#include "ws_receive.hpp"

namespace beauty {

class WsParser {
   public:
    WsParser(WsReceive &wsRecv);
    WsParser(const WsParser &) = delete;
    WsParser &operator=(const WsParser &) = delete;

    enum result_type { done, indeterminate };

    result_type parse();

   private:
    enum State {
        s_start,
        s_mask_and_len,
        s_ext_length,
        s_mask_1,
        s_mask_2,
        s_mask_3,
        s_mask_4,
        s_payload,
        s_close,
        s_ping,
        s_pong,
    } state_ = s_start;

    State getOpCodeState();

    result_type consume(std::vector<char>::iterator inPtr);
    bool isFin_;

    enum OpCode {
        Continuation = 0,
        TextData = 1,
        BinData = 2,
        Close = 8,
        Ping = 9,
        Pong = 10,
    } opCode_;

    size_t payloadLen_;
    bool hasMask_;
    int extLenBytes_;
    std::array<uint8_t, 4> mask_;
    size_t maskCounter_;
    WsReceive &wsRecv_;
};

}  // namespace beauty
