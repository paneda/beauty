#pragma once

#include <chrono>
#include <set>
#include <unordered_map>
#include <memory>

#include "beauty/connection.hpp"
#include "beauty/i_ws_sender.hpp"
#include "beauty/ws_types.hpp"

namespace beauty {

class WsEndpoint;

// Manages open connections so that they may be cleanly stopped when the server
// needs to shut down.
class ConnectionManager : public IWsSender {
   public:
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // Construct a connection manager.
    ConnectionManager(const Settings& settings);
    ~ConnectionManager() = default;

    // Add the specified connection to the manager and start it.
    void start(std::shared_ptr<Connection> c);

    // Stop the specified connection.
    void stop(std::shared_ptr<Connection> c);

    // Stop all connections.
    void stopAll();

    // Handle connections periodically.
    void tick();

    // Handler for debug messages.
    void setDebugMsgHandler(const debugMsgCallback& cb);

    // Connections may use the debug message handler.
    void debugMsg(const std::string& msg);

    // WebSocket endpoint management
    void setWsEndpoints(std::set<std::shared_ptr<WsEndpoint>> endpoints);
    WsEndpoint* getWsEndpointForPath(const std::string& path) const;

    // IWsSender interface implementation
    WriteResult sendWsText(const std::string& connectionId,
                           const std::string& message,
                           WriteCompleteCallback callback) override;
    WriteResult sendWsBinary(const std::string& connectionId,
                             const std::vector<char>& data,
                             WriteCompleteCallback callback) override;
    WriteResult sendWsClose(const std::string& connectionId,
                            uint16_t statusCode = 1000,
                            const std::string& reason = "",
                            WriteCompleteCallback callback = nullptr) override;
    std::vector<std::string> getActiveWsConnectionsForEndpoint(
        const IWsReceiver* endpoint) const override;
    bool isWriteInProgress(const std::string& connectionId) const override;

   private:
    // The managed connections.
    std::set<std::shared_ptr<Connection>> connections_;

    // WebSocket endpoint mapping (path -> endpoint)
    std::unordered_map<std::string, WsEndpoint*> pathToEndpoint_;

    // Settings for connections.
    const Settings& settings_;

    // Callback to handle debug messages.
    debugMsgCallback debugMsgCb_;
};

}  // namespace beauty
