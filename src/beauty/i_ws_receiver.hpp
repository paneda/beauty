#pragma once

#include <string>

#include "beauty/ws_message.hpp"

namespace beauty {

class IWsReceiver {
   public:
    IWsReceiver() = default;
    virtual ~IWsReceiver() = default;

    virtual void onWsOpen(const std::string& connectionId) = 0;
    virtual void onWsMessage(const std::string& connectionId, const WsMessage& wsMessage) = 0;
    virtual void onWsClose(const std::string& connectionId) = 0;
    virtual void onWsError(const std::string& connectionId, const std::string& error) = 0;
};

}  // namespace beauty
