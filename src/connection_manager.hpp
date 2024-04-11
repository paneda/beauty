#pragma once

#include <chrono>
#include <set>

#include "connection.hpp"

namespace http {
namespace server {

// Manages open connections so that they may be cleanly stopped when the server
// needs to shut down.
class ConnectionManager {
   public:
    ConnectionManager(const ConnectionManager &) = delete;
    ConnectionManager &operator=(const ConnectionManager &) = delete;

    // Construct a connection manager.
    ConnectionManager(HttpPersistence options);

    // Add the specified connection to the manager and start it.
    void start(std::shared_ptr<Connection> c);

    // Stop the specified connection.
    void stop(std::shared_ptr<Connection> c);

    // Stop all connections.
    void stopAll();

    // Set connection options.
    void setHttpPersistence(HttpPersistence options);

    // Handle connections periodically.
    void tick();

    // Handler for debug messages
    void setDebugMsgHandler(const debugMsgCallback &cb);

    // Connections may use the debug message handler.
    void debugMsg(const std::string &msg);

   private:
    // The managed connections.
    std::set<std::shared_ptr<Connection>> connections_;

    // Http persistence options.
    HttpPersistence httpPersistence_;

    // Callback to handle debug messages.
    debugMsgCallback debugMsgCb_;
};

}  // namespace server
}  // namespace http
