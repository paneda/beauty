#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "header.hpp"

namespace beauty {

// Received data over web socket.
struct WsReceiver {
    friend class Connection;
    friend class WsParser;

    WsReceiver(std::vector<char> &content) : content_(content) {}

    std::vector<char> &content_;

private:
	void reset() { outCounter_ = 0; }
	size_t outCounter_ = 0;
};

}  // namespace beauty
