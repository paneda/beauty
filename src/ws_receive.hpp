#pragma once

#include <vector>

namespace beauty {

// Received data over web socket.
struct WsReceive {
    friend class Connection;
    friend class WsParser;

    WsReceive(std::vector<char> &content) : content_(content) {}

    std::vector<char> &content_;

   private:
    void reset() {
        outCounter_ = 0;
    }
    size_t outCounter_ = 0;
};

}  // namespace beauty
