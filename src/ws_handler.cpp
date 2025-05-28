#include "ws_handler.hpp"

namespace beauty {

namespace {

void defaultWsOnOpenCallback(const std::string &) {}
void defaultWsOnCloseCallback(const std::string &) {}
void defaultWsOnMessageCallback(const std::string &, const WsReceive &) {}
void defaultWsOnErrorCallback(const std::string &, const std::string &) {}

}

WsHandler::WsHandler()
    : wsOnOpenCallback_(defaultWsOnOpenCallback),
      wsOnCloseCallback_(defaultWsOnCloseCallback),
      wsOnMessageCallback_(defaultWsOnMessageCallback),
      wsOnErrorCallback_(defaultWsOnErrorCallback) {}

void WsHandler::setWsOnOpenHandler(const wsOnOpenCallback &cb) {
    wsOnOpenCallback_ = cb;
}

void WsHandler::setWsOnCloseHandler(const wsOnCloseCallback &cb) {
    wsOnCloseCallback_ = cb;
}

void WsHandler::setWsOnMessageHandler(const wsOnMessageCallback &cb) {
    wsOnMessageCallback_ = cb;
}

void WsHandler::setWsOnErrorHandler(const wsOnErrorCallback &cb) {
    wsOnErrorCallback_ = cb;
}

void WsHandler::handleOnOpen(unsigned connectionId) {
    wsOnOpenCallback_(std::to_string(connectionId));
}

void WsHandler::handleOnClose(unsigned connectionId) {
    wsOnCloseCallback_(std::to_string(connectionId));
}

void WsHandler::handleOnMessage(unsigned connectionId, const WsReceive &recv) {
    wsOnMessageCallback_(std::to_string(connectionId), recv);
}

}  // namespace beauty
