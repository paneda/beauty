#pragma once

#include <string>
#include <vector>
#include "beauty/ws_types.hpp"

namespace beauty {

class IWsReceiver;

// Interface for sending WebSocket messages
//
// This interface is implemented by the ConnectionManager and provides
// WebSocket endpoints with the ability to send messages to specific
// connections.
class IWsSender {
   public:
    virtual ~IWsSender() = default;

    // Send a text message to a specific WebSocket connection
    // params:
    // connectionId: The connection ID to send to
    // message: The text message to send
    // callback: Callback for write completion notification
    // returns: WriteResult indicating success, write in progress, or connection closed
    virtual WriteResult sendWsText(const std::string& connectionId,
                                   const std::string& message,
                                   WriteCompleteCallback callback) = 0;

    // Send binary data to a specific WebSocket connection
    // params:
    // connectionId: The connection ID to send to
    // data: The binary data to send
    // callback: Callback for write completion notification
    // returns: WriteResult indicating success, write in progress, or connection closed
    virtual WriteResult sendWsBinary(const std::string& connectionId,
                                     const std::vector<char>& data,
                                     WriteCompleteCallback callback) = 0;

    // Send a close frame to a specific WebSocket connection
    // params:
    // connectionId: The connection ID to send to
    // statusCode: WebSocket close status code (default: 1000 = normal closure)
    // reason: Optional close reason string
    // callback: Callback for write completion notification
    // returns: WriteResult indicating success, write in progress, or connection closed
    virtual WriteResult sendWsClose(const std::string& connectionId,
                                    uint16_t statusCode = 1000,
                                    const std::string& reason = "",
                                    WriteCompleteCallback callback = nullptr) = 0;

    // Get list of active WebSocket connection IDs for a specific endpoint
    // params:
    // endpoint: The endpoint to get connections for (nullptr for all connections)
    // returns: Vector of connection ID strings
    virtual std::vector<std::string> getActiveWsConnectionsForEndpoint(
        const IWsReceiver* endpoint) const = 0;

    // Check if a connection is currently in the middle of a write operation
    // params:
    // connectionId: The connection ID to check
    // returns: true if write is in progress, false otherwise (or if connection not found)
    virtual bool isWriteInProgress(const std::string& connectionId) const = 0;
};

}  // namespace beauty