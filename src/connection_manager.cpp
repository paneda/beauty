#include "connection_manager.hpp"
#include <chrono>

namespace beauty {

ConnectionManager::ConnectionManager(HttpPersistence options)
    : httpPersistence_(options), debugMsgCb_(defaultDebugMsgHandler) {}

void ConnectionManager::start(std::shared_ptr<Connection> c) {
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

void ConnectionManager::stop(std::shared_ptr<Connection> c) {
    c->stop();
    connections_.erase(c);
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
	auto it = connections_.begin();
    while (it != connections_.end()) {
		bool erase = false;
		if ((*it)->isWebSocket()) {
			if (httpPersistence_.wsReceiveTimeout_ != std::chrono::seconds(0)) {
				if (((*it)->getLastReceivedTime() + httpPersistence_.wsReceiveTimeout_ < now)) {
					debugMsgCb_("Removing ws connection due to receive inactivity");
					erase = true;
				}
			}
		} else if ((*it)->useKeepAlive()) {
            if (((*it)->getLastReceivedTime() + httpPersistence_.keepAliveTimeout_ < now)) {
                debugMsgCb_("Removing connection due to inactivity");
                erase = true;
            }
            if ((*it)->getNrOfRequests() >= httpPersistence_.keepAliveMax_) {
                debugMsgCb_("Removing connection due max request limit");
                erase = true;
            }
        } 

		if (erase) {
			(*it)->stop();
			it = connections_.erase(it);
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
