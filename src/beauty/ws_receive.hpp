#pragma once

#include <vector>

namespace beauty {

// Received data over web socket.
struct WsReceive {
    friend class WsParser;

    WsReceive(std::vector<char> &content) : content_(content) {}

    void reset() {
        outCounter_ = 0;
    }

    std::vector<char> &content_;
    bool isFinal_ = false;

   private:
    size_t outCounter_ = 0;
    size_t payLoadCounter_ = 0;
};

}  // namespace beauty
