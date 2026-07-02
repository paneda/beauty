#include "beauty/ws_client.hpp"

#include <algorithm>

#include "beauty/i_random_interface.hpp"
#include "beauty/ws_sec_accept.hpp"

namespace beauty {

WsClient::WsClient(asio::io_context &ioContext,
                   IRandom &random,
                   IWsClientHandler &handler,
                   const Config &config)
    : ioContext_(ioContext),
      resolver_(ioContext),
      socket_(ioContext),
      reconnectTimer_(ioContext),
      pingTimer_(ioContext),
      random_(random),
      handler_(handler),
      config_(config),
      response_(bodyBuffer_),
      wsEncoder_(sendBuffer_, random),
      wsMessage_(recvBuffer_),
      wsParser_(wsMessage_),
      currentReconnectDelay_(config.reconnectInitialDelay) {
    recvBuffer_.reserve(config_.maxMessageSize);
    sendBuffer_.reserve(config_.maxMessageSize);
    bodyBuffer_.reserve(config_.maxMessageSize);
}

void WsClient::connect(const std::string &urlStr) {
    if (!url_.parse(urlStr)) {
        handler_.onWsError("Invalid WebSocket URL");
        return;
    }

    host_ = url_.hostname();
    port_ = std::to_string(url_.httpPort());
    path_ = url_.path();
    if (path_.empty()) {
        path_ = "/";
    }
    if (!url_.query().empty()) {
        path_ += "?" + url_.query();
    }

    reconnectEnabled_ = config_.autoReconnect;
    currentReconnectDelay_ = config_.reconnectInitialDelay;

    std::error_code ignore;
    socket_.close(ignore);

    resetForNewConnection();
    doResolve();
}

void WsClient::resetForNewConnection() {
    responseParser_.reset();
    response_.reset();
    wsParser_.reset();
    recvBuffer_.clear();
    sendBuffer_.clear();
    bodyBuffer_.clear();
    isOpen_ = false;
    handshakeDone_ = false;
    writeInProgress_ = false;
    closing_ = false;
    finishing_ = false;
    closePending_ = false;
    wsParseOffset_ = 0;
}

void WsClient::doResolve() {
    auto self(shared_from_this());
    resolver_.async_resolve(host_,
                            port_,
                            [this, self](const std::error_code &ec,
                                         const asio::ip::tcp::resolver::results_type &endpoints) {
                                if (ec) {
                                    handleDisconnect("Resolve failed: " + ec.message());
                                    return;
                                }
                                doConnect(endpoints);
                            });
}

void WsClient::doConnect(const asio::ip::tcp::resolver::results_type &endpoints) {
    auto self(shared_from_this());
    asio::async_connect(socket_,
                        endpoints,
                        [this, self](const std::error_code &ec, const asio::ip::tcp::endpoint &) {
                            if (ec) {
                                handleDisconnect("Connect failed: " + ec.message());
                                return;
                            }
                            doWriteHandshake();
                        });
}

void WsClient::doWriteHandshake() {
    secKey_ = generateWsSecKey(random_);

    std::string request;
    request.reserve(256);
    request += "GET " + path_ + " HTTP/1.1\r\n";
    request += "Host: " + host_ + ":" + port_ + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + secKey_ + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "\r\n";

    sendBuffer_.assign(request.begin(), request.end());

    auto self(shared_from_this());
    asio::async_write(
        socket_, asio::buffer(sendBuffer_), [this, self](const std::error_code &ec, std::size_t) {
            if (ec) {
                handleDisconnect("Handshake write failed: " + ec.message());
                return;
            }
            doReadHandshake();
        });
}

void WsClient::doReadHandshake() {
    auto self(shared_from_this());
    recvBuffer_.resize(config_.maxMessageSize);
    socket_.async_read_some(
        asio::buffer(recvBuffer_),
        [this, self](const std::error_code &ec, std::size_t bytesTransferred) {
            if (ec) {
                handleDisconnect("Handshake read failed: " + ec.message());
                return;
            }
            recvBuffer_.resize(bytesTransferred);

            ResponseParser::result_type result = responseParser_.parse(response_, recvBuffer_);

            if (result == ResponseParser::switching_protocols) {
                std::string accept = response_.getHeaderValue("Sec-WebSocket-Accept");
                if (!verifyWsSecAccept(secKey_, accept)) {
                    handleDisconnect("Invalid Sec-WebSocket-Accept in handshake response");
                    return;
                }
                handshakeDone_ = true;
                isOpen_ = true;
                currentReconnectDelay_ = config_.reconnectInitialDelay;
                handler_.onWsOpen();

                startPingTimer();

                // Any bytes left in recvBuffer_ are the start of WebSocket
                // frames that arrived together with the handshake response.
                if (isOpen_ && !recvBuffer_.empty()) {
                    processWsBuffer();
                } else if (isOpen_) {
                    doReadWs();
                }
            } else if (result == ResponseParser::good_part) {
                // Need more handshake data.
                doReadHandshake();
            } else {
                // good_complete (non-101), bad, version_not_supported, ...
                handleDisconnect("WebSocket handshake rejected (status " +
                                 std::to_string(response_.statusCode_) + ")");
            }
        });
}

void WsClient::processWsBuffer() {
    if (wsParseOffset_ == 0) {
        wsMessage_.reset();
    }
    WsParser::result_type result = wsParser_.parse(wsParseOffset_);

    if (result == WsParser::data_frame) {
        wsParseOffset_ = 0;
        handler_.onWsMessage(wsMessage_);
        if (isOpen_) {
            doReadWs();
        }
    } else if (result == WsParser::indeterminate) {
        // Partial frame — remember how many decoded bytes are in the buffer so
        // the next read appends after them and the parser can resume.
        wsParseOffset_ = recvBuffer_.size();
        if (isOpen_) {
            doReadWs();
        }
    } else if (result == WsParser::ping_frame) {
        wsParseOffset_ = 0;
        wsEncoder_.encodePongFrame(wsMessage_.content_);
        doWriteWsFrame(true, nullptr);
    } else if (result == WsParser::pong_frame) {
        wsParseOffset_ = 0;
        if (isOpen_) {
            doReadWs();
        }
    } else if (result == WsParser::close_frame) {
        wsParseOffset_ = 0;
        isOpen_ = false;
        finishConnection(true);
    } else if (result == WsParser::fragmentation_error) {
        wsParseOffset_ = 0;
        handler_.onWsError("Fragmented messages are not supported");
        if (isOpen_) {
            isOpen_ = false;
            wsEncoder_.encodeCloseFrame(1003, "Fragmented messages not supported");
            doWriteWsFrame(
                false, [this](const std::error_code &, std::size_t) { finishConnection(true); });
        }
    }
}

void WsClient::doReadWs() {
    auto self(shared_from_this());
    // When resuming a partial frame, wsParseOffset_ decoded payload bytes
    // occupy the front of recvBuffer_. Read new data after them.
    size_t readSize = config_.maxMessageSize - wsParseOffset_;
    recvBuffer_.resize(wsParseOffset_ + readSize);
    socket_.async_read_some(asio::buffer(recvBuffer_.data() + wsParseOffset_, readSize),
                            [this, self](const std::error_code &ec, std::size_t bytesTransferred) {
                                if (ec) {
                                    handleDisconnect("Read failed: " + ec.message());
                                    return;
                                }
                                recvBuffer_.resize(wsParseOffset_ + bytesTransferred);
                                processWsBuffer();
                            });
}

void WsClient::doWriteWsFrame(bool continueReading, WriteCompleteCallback callback) {
    writeInProgress_ = true;
    auto self(shared_from_this());
    asio::async_write(socket_,
                      asio::buffer(sendBuffer_),
                      [this, self, continueReading, callback](const std::error_code &ec,
                                                              std::size_t bytesWritten) {
                          writeInProgress_ = false;
                          if (ec) {
                              if (callback) {
                                  callback(ec, bytesWritten);
                              }
                              handleDisconnect("Write failed: " + ec.message());
                              return;
                          }
                          if (callback) {
                              callback(ec, bytesWritten);
                          }
                          // If close() was called while this write was in
                          // flight, now send the close frame.
                          if (closePending_ && !closing_) {
                              closePending_ = false;
                              isOpen_ = false;
                              closing_ = true;
                              wsEncoder_.encodeCloseFrame(pendingCloseCode_, pendingCloseReason_);
                              doWriteWsFrame(false,
                                             [this, self](const std::error_code &, std::size_t) {
                                                 finishConnection(true);
                                             });
                              return;
                          }
                          if (continueReading && isOpen_) {
                              doReadWs();
                          }
                      });
}

WriteResult WsClient::sendText(const std::string &text) {
    if (!isOpen_) {
        return WriteResult::CONNECTION_CLOSED;
    }
    if (writeInProgress_) {
        return WriteResult::WRITE_IN_PROGRESS;
    }
    wsEncoder_.encodeTextFrame(text);
    doWriteWsFrame(false, nullptr);
    return WriteResult::SUCCESS;
}

WriteResult WsClient::sendBinary(const std::vector<char> &data) {
    if (!isOpen_) {
        return WriteResult::CONNECTION_CLOSED;
    }
    if (writeInProgress_) {
        return WriteResult::WRITE_IN_PROGRESS;
    }
    wsEncoder_.encodeBinaryFrame(data);
    doWriteWsFrame(false, nullptr);
    return WriteResult::SUCCESS;
}

void WsClient::sendPing() {
    if (!isOpen_ || writeInProgress_) {
        return;
    }
    wsEncoder_.encodePingFrame();
    doWriteWsFrame(false, nullptr);
}

void WsClient::close(uint16_t statusCode, const std::string &reason) {
    reconnectEnabled_ = false;
    if (!isOpen_) {
        return;
    }
    if (writeInProgress_) {
        // Defer the close frame until the current write completes.
        closePending_ = true;
        pendingCloseCode_ = statusCode;
        pendingCloseReason_ = reason;
        return;
    }
    isOpen_ = false;
    closing_ = true;
    wsEncoder_.encodeCloseFrame(statusCode, reason);
    doWriteWsFrame(false, [this](const std::error_code &, std::size_t) { finishConnection(true); });
}

void WsClient::startPingTimer() {
    if (config_.pingInterval.count() == 0) {
        return;
    }
    auto self(shared_from_this());
    pingTimer_.expires_after(config_.pingInterval);
    pingTimer_.async_wait([this, self](const std::error_code &ec) {
        if (ec || !isOpen_) {
            return;
        }
        sendPing();
        startPingTimer();
    });
}

void WsClient::finishConnection(bool notifyClose) {
    if (finishing_) {
        return;
    }
    finishing_ = true;
    isOpen_ = false;

    std::error_code ignore;
    pingTimer_.cancel();
    socket_.close(ignore);

    if (notifyClose) {
        handler_.onWsClose();
    }

    if (reconnectEnabled_) {
        scheduleReconnect();
    }
}

void WsClient::handleDisconnect(const std::string &error) {
    if (finishing_) {
        return;
    }
    finishing_ = true;
    bool wasOpen = isOpen_;
    isOpen_ = false;

    std::error_code ignore;
    pingTimer_.cancel();
    socket_.close(ignore);

    handler_.onWsError(error);
    if (wasOpen) {
        handler_.onWsClose();
    }

    if (reconnectEnabled_) {
        scheduleReconnect();
    }
}

void WsClient::scheduleReconnect() {
    auto self(shared_from_this());
    reconnectTimer_.expires_after(currentReconnectDelay_);

    // Exponential backoff, capped at the configured maximum.
    std::chrono::milliseconds next = currentReconnectDelay_ * 2;
    if (next > config_.reconnectMaxDelay) {
        next = config_.reconnectMaxDelay;
    }
    currentReconnectDelay_ = next;

    reconnectTimer_.async_wait([this, self](const std::error_code &ec) {
        if (ec) {
            return;
        }
        resetForNewConnection();
        doResolve();
    });
}

}  // namespace beauty
