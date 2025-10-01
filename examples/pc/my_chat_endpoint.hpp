#pragma once

#include <iostream>
#include <string>
#include <beauty/ws_endpoint.hpp>

// Example of a simple WebSocket chat endpoint
class MyChatEndpoint : public beauty::WsEndpoint {
   public:
    MyChatEndpoint() : WsEndpoint("/ws/chat") {}

    void onWsOpen(const std::string& connectionId) override {
        std::cout << "Chat client connected: " << connectionId << std::endl;
        sendText(connectionId,
                 "{\"type\":\"welcome\",\"message\":\"Welcome to the Beauty demo chat room!\"}",
                 nullptr);

        // Notify others, send to all clients except the new one
        for (const auto& connId : getActiveConnections()) {
            if (connId != connectionId) {
                sendText(connId,
                         "{\"type\":\"user_joined\",\"user\":\"" + connectionId + "\"}",
                         nullptr);
            }
        }
    }

    void onWsMessage(const std::string& connectionId, const beauty::WsMessage& wsMessage) override {
        std::string message(wsMessage.content_.begin(), wsMessage.content_.end());
        std::cout << "Chat message from " << connectionId << ": " << message << std::endl;

        // Send message to all clients except the sender
        for (const auto& connId : getActiveConnections()) {
            if (connId != connectionId) {
                sendText(connId,
                         "{\"type\":\"chat_message\",\"from\":\"" + connectionId +
                             "\",\"message\":\"" + message + "\"}",
                         nullptr);
            }
        }
    }

    void onWsClose(const std::string& connectionId) override {
        std::cout << "Chat client disconnected: " << connectionId << std::endl;
        // Notify remaining clients about disconnection
        for (const auto& connId : getActiveConnections()) {
            sendText(connId, "{\"type\":\"user_left\",\"user\":\"" + connectionId + "\"}", nullptr);
        }
    }

    void onWsError(const std::string& connectionId, const std::string& error) override {
        std::cout << "Chat endpoint error for " << connectionId << ": " << error << std::endl;
    }
};
