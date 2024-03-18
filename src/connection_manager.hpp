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
    void start(connection_ptr c);

    // Stop the specified connection.
    void stop(connection_ptr c);

    // Stop all connections.
    void stopAll();

    // Set connection options.
    void setHttpPersistence(HttpPersistence options);

    // Handle connections periodically.
    void tick();

   private:
    // The managed connections.
    std::set<connection_ptr> connections_;

    // Http persistence options.
    HttpPersistence httpPersistence_;
};

}  // namespace server
}  // namespace http
