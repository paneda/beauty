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

    void setWsOnOpenHandler(const wsOnOpenCallback &cb);
    void setWsOnCloseHandler(const wsOnCloseCallback &cb);
    void setWsOnMessageHandler(const wsOnMessageCallback &cb);
    void setWsOnErrorHandler(const wsOnErrorCallback &cb);

    void handleOnOpen(unsigned connectionId);
    void handleOnClose(unsigned connectionId);
    void handleOnMessage(unsigned connectionId, const WsReceive &recv);

   private:
    wsOnOpenCallback wsOnOpenCallback_;
    wsOnCloseCallback wsOnCloseCallback_;
    wsOnMessageCallback wsOnMessageCallback_;
    wsOnErrorCallback wsOnErrorCallback_;
};

}  // namespace beauty
