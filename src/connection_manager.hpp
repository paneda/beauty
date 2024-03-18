#pragma once

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
    ConnectionManager();

    // Add the specified connection to the manager and start it.
    void start(connection_ptr c);

    // Stop the specified connection.
    void stop(connection_ptr c);

    // Stop all connections.
    void stopAll();

   private:
    // The managed connections.
    std::set<connection_ptr> connections_;
};

}  // namespace server
}  // namespace http
