#include "connection_manager.hpp"

#include <iostream>

using namespace std::literals::chrono_literals;

namespace http {
namespace server {

ConnectionManager::ConnectionManager(HttpPersistence options) : httpPersistence_(options) {}

void ConnectionManager::start(connection_ptr c) {
    connections_.insert(c);
    std::chrono::seconds keepAliveTimeout = 0s;
    if (httpPersistence_.connectionLimit_ == 0 ||
        (httpPersistence_.connectionLimit_ > 0 &&
         connections_.size() <= httpPersistence_.connectionLimit_)) {
        keepAliveTimeout = httpPersistence_.keepAliveTimeout_;
    }
    c->start(keepAliveTimeout, httpPersistence_.keepAliveMax_);
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
        if (((*it)->getLastRequestTime() + httpPersistence_.keepAliveTimeout_ < now) ||
            ((*it)->getNrOfRequests() >= httpPersistence_.keepAliveMax_)) {
            (*it)->stop();
            connections_.erase(it++);
            std::cout << "erasing connection\n";
        } else {
            ++it;
        }
    }
}

}  // namespace server
}  // namespace http
