#include "ws_handler.hpp"

namespace beauty {

namespace {

void defaultWsClientCallback(const WsClientEvent event, const WsReceive &recv) {}

}

WsHandler::WsHandler() : wsClientCallback_(defaultWsClientCallback) {}

void WsHandler::setWsClientHandler(const wsClientCallback &cb) {
    wsClientCallback_ = cb;
}

void WsHandler::handleOnOpen(unsigned connectionId, const WsReceive &recv) {
	wsClientCallback_(WsClientEvent::onOpen, recv);
}

}  // namespace beauty
