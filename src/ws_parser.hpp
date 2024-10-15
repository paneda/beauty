#pragma once

#include <array>
#include <vector>

#include "ws_receiver.hpp"

namespace beauty {

class WsParser {
   public:
	WsParser(WsReceiver &wsRecv);
    WsParser(const WsParser &) = delete;
    WsParser &operator=(const WsParser &) = delete;
    WsParser() = default;

    enum result_type { done, bad, indeterminate };

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
    } state_ = s_start;

	result_type consume(std::vector<char>::iterator inPtr);
	bool isFin_;

	enum OpCodeDataFrame {
		Continuation = 0,
		TextData = 1,
		BinData = 2,
	} opCodeDF_; 

	enum OpCodeControlFrame {
		Close = 8,
		Ping = 9,
		Pong = 10,
	} opCodeCF_; 

	size_t payloadLen_;
	bool hasMask_;
	int extLenBytes_;
	std::array<uint8_t, 4> mask_;
	WsReceiver& wsRecv_;
};

}  // namespace beauty
