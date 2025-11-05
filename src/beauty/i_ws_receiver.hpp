#pragma once

#include <string>

#include "beauty/ws_message.hpp"

namespace beauty {

// Interface for receiving WebSocket events
// This interface should be implemented by WebSocket endpoint classes
// to handle events such as connection open, message received, connection
// close, and errors.
class IWsReceiver {
   public:
    IWsReceiver() = default;
    virtual ~IWsReceiver() = default;

    // Called when a new WebSocket connection is opened
    // params:
    // connectionId: The ID of the connection
    virtual void onWsOpen(const std::string& connectionId) = 0;

    // Called when a WebSocket message is received
    // params:
    // connectionId: The ID of the connection
    // wsMessage: The received WebSocket message
    virtual void onWsMessage(const std::string& connectionId, const WsMessage& wsMessage) = 0;

    // Called when a WebSocket connection is closed
    // params:
    // connectionId: The ID of the connection
    virtual void onWsClose(const std::string& connectionId) = 0;

    // Called when a WebSocket error occurs
    // params:
    // connectionId: The ID of the connection
    // error: The error message
    virtual void onWsError(const std::string& connectionId, const std::string& error) = 0;
};

}  // namespace beauty
