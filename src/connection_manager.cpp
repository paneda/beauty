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
        if ((*it)->useKeepAlive()) {
            bool erase = false;
            if (((*it)->getLastActivityTime() + settings_.keepAliveTimeout_ < now)) {
                debugMsgCb_("Removing connection due to inactivity");
                erase = true;
            }
            if ((*it)->getNrOfRequests() >= settings_.keepAliveMax_) {
                debugMsgCb_("Removing connection due max request limit");
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
