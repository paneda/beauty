#include "connection_manager.hpp"

namespace http {
namespace server {

ConnectionManager::ConnectionManager(HttpPersistence options)
    : httpPersistence_(options), debugMsgCb_(defaultDebugMsgHandler) {}

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
    // connections_.erase(
    //     std::remove_if(connections_.begin(), connections_.end(), [&](connection_ptr c) {
    //         bool erase = false;
    //         if ((c->getLastReceivedTime() + httpPersistence_.keepAliveTimeout_ < now)) {
    //             debugMsgCb_("Removing connection due to inactivity");
    //             erase = true;
    //         }
    //         if (c->getNrOfRequests() >= httpPersistence_.keepAliveMax_) {
    //             debugMsgCb_("Removing connection due max request limit");
    //             erase = true;
    //         }

    //         if (erase) {
    //             c->stop();
    //         }
    //         return erase;
    //     }));
    debugMsg(std::to_string(connections_.size()));
    auto it = connections_.begin();
    while (it != connections_.end()) {
        if ((*it)->useKeepAlive()) {
            bool erase = false;
            if (((*it)->getLastReceivedTime() + httpPersistence_.keepAliveTimeout_ < now)) {
                debugMsgCb_("Removing connection due to inactivity");
                erase = true;
            }
            if ((*it)->getNrOfRequests() >= httpPersistence_.keepAliveMax_) {
                debugMsgCb_("Removing connection due max request limit");
                erase = true;
            }

            if (erase) {
                (*it)->stop();
                debugMsgCb_("erasing..");
                it = connections_.erase(it);
            } else {
                it++;
            }
        } else {
            it++;
        }
    }
}

void ConnectionManager::setDebugMsgHandler(const debugMsgCallback &cb) {
    debugMsgCb_ = cb;
}

void ConnectionManager::debugMsg(const std::string &msg) {
    debugMsgCb_(msg);
}

}  // namespace server
}  // namespace http
