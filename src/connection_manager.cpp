#include <chrono>
#include <iostream>

#include "connection_manager.hpp"

namespace http {
namespace server {

ConnectionManager::ConnectionManager(HttpPersistence options) : httpPersistence_(options) {}

void ConnectionManager::start(connection_ptr c) {
    connections_.insert(c);
    bool useKeepAlive = false;
    if (httpPersistence_.keepAliveTimeout_ != std::chrono::seconds(0) &&
        (httpPersistence_.connectionLimit_ == 0 ||  // 0 = unlimited
         (httpPersistence_.connectionLimit_ > 0 &&
          connections_.size() <= httpPersistence_.connectionLimit_))) {
        useKeepAlive = true;
    }
    c->start(useKeepAlive, httpPersistence_.keepAliveTimeout_, httpPersistence_.keepAliveMax_);
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

void ConnectionManager::setHttpPersistence(HttpPersistence options) {
    httpPersistence_ = options;
}

void ConnectionManager::tick() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = connections_.begin(); it != connections_.end();) {
        if ((*it)->useKeepAlive()) {
            bool erase = false;
            if (((*it)->getLastReceivedTime() + httpPersistence_.keepAliveTimeout_ < now)) {
                std::cout << "Removing connection due to inactivity\n";
                erase = true;
            }
            if ((*it)->getNrOfRequests() >= httpPersistence_.keepAliveMax_) {
                std::cout << "Removing connection due max request limit\n";
                erase = true;
            }

            if (erase) {
                (*it)->stop();
                connections_.erase(it);
            }
        }
        ++it;
    }
}

}  // namespace server
}  // namespace http
