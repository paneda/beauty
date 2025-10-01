#pragma once

#include <string>
#include <vector>
#include "beauty/i_ws_receiver.hpp"
#include "beauty/i_ws_sender.hpp"

namespace beauty {

// WebSocket endpoint that handles connections on a specific path
//
// This class provides an API for handling WebSocket connections on a specific
// path (e.g., "/chat", "/api/data"). Users inherit from this class and
// override the IWsReceiver methods to handle WebSocket events, and can use the
// sending methods to send messages to clients.
//
// Example usage:
// class ChatEndpoint : public WsEndpoint {
// public:
//     ChatEndpoint() : WsEndpoint("/chat") {}
//
//     void onWsMessage(const std::string& connectionId, const WsMessage& msg) override {
//         // Echo the message back
//         sendText(connectionId, "Echo: " + msg.content_, nullptr);
//     }
// };
class WsEndpoint : public IWsReceiver {
   private:
    std::string path_;
    IWsSender* wsSender_;

   public:
    // Construct a WebSocket endpoint for a specific path
    // params:
    // path: The URL path this endpoint handles (e.g., "/chat", "/api/data")
    explicit WsEndpoint(const std::string& path) : path_(path), wsSender_(nullptr) {}

    virtual ~WsEndpoint() = default;

    // Get the path of this endpoint
    // returns: The URL path string
    const std::string& getPath() const {
        return path_;
    }

    // Send a text message with callback and state tracking
    // params:
    // connectionId: The connection ID to send to
    // message: The text message to send
    // callback: Callback for write completion notification
    // returns: WriteResult indicating success, write in progress, or connection closed
    WriteResult sendText(const std::string& connectionId,
                         const std::string& message,
                         WriteCompleteCallback callback) {
        return wsSender_ ? wsSender_->sendWsText(connectionId, message, callback)
                         : WriteResult::CONNECTION_CLOSED;
    }

    // Send binary data with callback and state tracking
    // params:
    // connectionId: The connection ID to send to
    // data: The binary data to send
    // callback: Callback for write completion notification
    // returns: WriteResult indicating success, write in progress, or connection closed
    WriteResult sendBinary(const std::string& connectionId,
                           const std::vector<char>& data,
                           WriteCompleteCallback callback) {
        return wsSender_ ? wsSender_->sendWsBinary(connectionId, data, callback)
                         : WriteResult::CONNECTION_CLOSED;
    }

    // Send close frame with callback and state tracking
    // params:
    // connectionId: The connection ID to send to
    // statusCode: WebSocket close status code (default: 1000 = normal closure)
    // reason: Optional close reason string
    // callback: Callback for write completion notification
    // returns: WriteResult indicating success, write in progress, or connection closed
    WriteResult sendClose(const std::string& connectionId,
                          uint16_t statusCode,
                          const std::string& reason,
                          WriteCompleteCallback callback) {
        return wsSender_ ? wsSender_->sendWsClose(connectionId, statusCode, reason, callback)
                         : WriteResult::CONNECTION_CLOSED;
    }

    // Get list of active connection IDs for this endpoint
    // returns: Vector of connection ID strings
    std::vector<std::string> getActiveConnections() const {
        return wsSender_ ? wsSender_->getActiveWsConnectionsForEndpoint(this)
                         : std::vector<std::string>();
    }

    // Check if a connection is available to send (not in the middle of a write)
    // params:
    // connectionId: The connection ID to check
    // returns: true if connection can send data, false if write is in progress or connection not
    // found
    bool canSendTo(const std::string& connectionId) const {
        return wsSender_ ? !wsSender_->isWriteInProgress(connectionId) : false;
    }

    // IWsReceiver interface - to be implemented by derived classes
    virtual void onWsOpen(const std::string& connectionId) override = 0;
    virtual void onWsMessage(const std::string& connectionId,
                             const WsMessage& wsMessage) override = 0;
    virtual void onWsClose(const std::string& connectionId) override = 0;
    virtual void onWsError(const std::string& connectionId, const std::string& error) override = 0;

   private:
    friend class ConnectionManager;
    void setWsSender(IWsSender* sender) {
        wsSender_ = sender;
    }
};

}  // namespace beauty