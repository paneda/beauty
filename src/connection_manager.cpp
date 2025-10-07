#include "beauty/connection_manager.hpp"
#include <chrono>

namespace {
void defaultDebugMsgHandler(const std::string &) {}
}

namespace beauty {

ConnectionManager::ConnectionManager(const Settings &settings)
    : settings_(settings), debugMsgCb_(defaultDebugMsgHandler) {}

void ConnectionManager::start(std::shared_ptr<Connection> c) {
    connections_.insert(c);
    bool useKeepAlive = false;
    if (settings_.keepAliveTimeout_ != std::chrono::seconds(0) &&
        (settings_.connectionLimit_ == 0 ||  // 0 = unlimited
         (settings_.connectionLimit_ > 0 && connections_.size() <= settings_.connectionLimit_))) {
        useKeepAlive = true;
    }
    c->start(useKeepAlive, settings_.keepAliveTimeout_, settings_.keepAliveMax_);
}

void ConnectionManager::stop(std::shared_ptr<Connection> c) {
    connections_.erase(c);
    c->stop();
}

void ConnectionManager::stopAll() {
    for (auto c : connections_) {
        c->stop();
    }
    connections_.clear();
}

void ConnectionManager::tick() {
    auto now = std::chrono::steady_clock::now();
    auto it = connections_.begin();
    while (it != connections_.end()) {
        if ((*it)->isWebSocket()) {
            // Check WebSocket timeouts
            if (settings_.wsReceiveTimeout_ != std::chrono::seconds(0) &&
                ((*it)->getLastReceivedTime() + settings_.wsReceiveTimeout_ < now)) {
                debugMsgCb_("Removing WebSocket connection due to receive timeout");
                (*it)->stop();
                it = connections_.erase(it);
                continue;
            }
            if (settings_.wsPingInterval_ != std::chrono::seconds(0) &&
                ((*it)->getLastPingTime() + settings_.wsPingInterval_ < now)) {
                // Time to send ping
                (*it)->sendWsPing();
            }
            if (settings_.wsPongTimeout_ != std::chrono::seconds(0) &&
                ((*it)->getLastPingTime() + settings_.wsPongTimeout_ < now) &&
                ((*it)->getLastPongTime() < (*it)->getLastPingTime())) {
                debugMsgCb_("Removing WebSocket connection due to pong timeout");
                (*it)->stop();
                it = connections_.erase(it);
                continue;
            }
            it++;
            continue;
        } else if ((*it)->useKeepAlive()) {
            bool erase = false;
            if (((*it)->getLastActivityTime() + settings_.keepAliveTimeout_ < now)) {
                debugMsgCb_("Removing HTTP connection due to inactivity");
                erase = true;
            }
            if ((*it)->getNrOfRequests() >= settings_.keepAliveMax_) {
                debugMsgCb_("Removing HTTP connection due max request limit");
                erase = true;
            }

            if (erase) {
                (*it)->stop();
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

}  // namespace beauty
