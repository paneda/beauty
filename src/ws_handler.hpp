#pragma once

#include "beauty_common.hpp"
// #include "WsSend.hpp"
#include "ws_receive.hpp"

namespace beauty {

struct WsSend;
struct WsReceive;

class WsHandler {
   public:
    WsHandler(const WsHandler &) = delete;
    WsHandler &operator=(const WsHandler &) = delete;

    WsHandler();

    // Handlers to be optionally implemented.
    void setWsClientHandler(const wsClientCallback &cb);

    void handleOnOpen(unsigned connectionId, const WsReceive &recv);
    void handleOnClose(unsigned connectionId, const WsReceive &recv);
    void handleOnMessage(unsigned connectionId, const WsReceive &recv);

   private:
    wsClientCallback wsClientCallback_;
};

}  // namespace beauty
