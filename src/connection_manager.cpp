#include "beauty/connection_manager.hpp"
#include "beauty/ws_endpoint.hpp"
#include <chrono>

namespace {
void defaultDebugMsgHandler(const std::string&) {}
}

namespace beauty {

ConnectionManager::ConnectionManager(const Settings& settings)
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

void ConnectionManager::setDebugMsgHandler(const debugMsgCallback& cb) {
    debugMsgCb_ = cb;
}

void ConnectionManager::debugMsg(const std::string& msg) {
    debugMsgCb_(msg);
}

WriteResult ConnectionManager::sendWsText(const std::string& connectionId,
                                          const std::string& message,
                                          WriteCompleteCallback callback) {
    for (auto& conn : connections_) {
        if (conn->isWebSocket() && std::to_string(conn->getConnectionId()) == connectionId) {
            return conn->sendWsText(message, callback);
        }
    }
    return WriteResult::CONNECTION_CLOSED;  // Connection not found or not a WebSocket
}

WriteResult ConnectionManager::sendWsBinary(const std::string& connectionId,
                                            const std::vector<char>& data,
                                            WriteCompleteCallback callback) {
    for (auto& conn : connections_) {
        if (conn->isWebSocket() && std::to_string(conn->getConnectionId()) == connectionId) {
            return conn->sendWsBinary(data, callback);
        }
    }
    return WriteResult::CONNECTION_CLOSED;  // Connection not found or not a WebSocket
}

WriteResult ConnectionManager::sendWsClose(const std::string& connectionId,
                                           uint16_t statusCode,
                                           const std::string& reason,
                                           WriteCompleteCallback callback) {
    for (auto& conn : connections_) {
        if (conn->isWebSocket() && std::to_string(conn->getConnectionId()) == connectionId) {
            return conn->sendWsClose(statusCode, reason, callback);
        }
    }
    return WriteResult::CONNECTION_CLOSED;  // Connection not found or not a WebSocket
}

bool ConnectionManager::isWriteInProgress(const std::string& connectionId) const {
    for (const auto& conn : connections_) {
        if (conn->isWebSocket() && std::to_string(conn->getConnectionId()) == connectionId) {
            return conn->isWriteInProgress();
        }
    }
    return false;  // Connection not found or not a WebSocket
}

void ConnectionManager::setWsEndpoints(std::set<std::shared_ptr<WsEndpoint>> endpoints) {
    pathToEndpoint_.clear();
    for (const auto& ep : endpoints) {
        pathToEndpoint_[ep->getPath()] = ep.get();
        ep->setWsSender(this);
    }
}

WsEndpoint* ConnectionManager::getWsEndpointForPath(const std::string& path) const {
    auto it = pathToEndpoint_.find(path);
    return (it != pathToEndpoint_.end()) ? it->second : nullptr;
}

// IWsSender interface implementation - endpoint-specific methods

std::vector<std::string> ConnectionManager::getActiveWsConnectionsForEndpoint(
    const IWsReceiver* endpoint) const {
    std::vector<std::string> wsConnections;
    for (const auto& conn : connections_) {
        if (conn->isWebSocket() && conn->getWsEndpoint() == endpoint) {
            wsConnections.push_back(std::to_string(conn->getConnectionId()));
        }
    }
    return wsConnections;
}

}  // namespace beauty
