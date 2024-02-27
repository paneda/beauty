#include "connection_manager.hpp"

namespace http {
namespace server {

ConnectionManager::ConnectionManager() {}

void ConnectionManager::start(connection_ptr c) {
    connections_.insert(c);
    c->start();
}

void ConnectionManager::stop(connection_ptr c) {
    connections_.erase(c);
    c->stop();
}

void ConnectionManager::stopAll() {
    for (auto c : connections_) {
        c->stop();
    }
    connections_.clear();
}

}  // namespace server
}  // namespace http
