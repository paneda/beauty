#include "connection_manager.hpp"
#include "connection.hpp"
#include <iostream>

namespace beauty {

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionManager &manager,
                       RequestHandler &requestHandler,
                       WsHandler &wsHandler,
                       unsigned connectionId,
                       size_t maxContentSize)
    : socket_(std::move(socket)),
      connectionManager_(manager),
      requestHandler_(requestHandler),
      wsHandler_(wsHandler),
      connectionId_(connectionId),
      maxContentSize_(maxContentSize),
      buffer_(maxContentSize),
      request_(buffer_),
      reply_(maxContentSize),
      wsRecv_(buffer_),
      wsParser_(wsRecv_) {}

void Connection::start(bool useKeepAlive,
                       std::chrono::seconds keepAliveTimeout,
                       size_t keepAliveMax) {
    lastReceivedTime_ = std::chrono::steady_clock::now();
    useKeepAlive_ = useKeepAlive;
    keepAliveTimeout_ = keepAliveTimeout;
    keepAliveMax_ = keepAliveMax;
    doRead();
}

void Connection::stop() {
    if (isWebSocket_) {
        wsHandler_.handleOnClose(connectionId_);
    }
    socket_.close();
}

std::chrono::steady_clock::time_point Connection::getLastReceivedTime() const {
    return lastReceivedTime_;
}

size_t Connection::getNrOfRequests() const {
    return nrOfRequest_;
}

bool Connection::useKeepAlive() const {
    return ((useKeepAlive_ && request_.keepAlive_) || isWebSocket_);
}

bool Connection::isWebSocket() const {
	return isWebSocket_;
}

void Connection::doRead() {
    auto self(shared_from_this());
    // asio uses buffer_.size() to limit amount of read data so must restore
    // size before reading. Note: operation is "cheap" as maxContentSize is
    // already reserved.
    buffer_.resize(maxContentSize_);
    socket_.async_read_some(
        asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
            if (!ec) {
                lastReceivedTime_ = std::chrono::steady_clock::now();
                buffer_.resize(bytesTransferred);
                if (isWebSocket_) {
                    printf("bytesTr: %d\n", bytesTransferred);
                    for (int i = 0; i < bytesTransferred; ++i) {
                        printf("0x%.2x, ", (uint8_t)buffer_[i]);
                    }
                    puts("}");
                    wsRecv_.reset();
                    WsParser::result_type result = wsParser_.parse();
					wsHandler_.handleOnMessage(connectionId_, wsRecv_);
                    doRead();
                } else {
                    RequestParser::result_type result = requestParser_.parse(request_, buffer_);
                    if (result == RequestParser::good_complete) {
                        std::string hVal = request_.getHeaderValue("connection");
                        if (strcasecmp(hVal.c_str(), "upgrade") == 0) {
                            if (handleUpgradeRequest()) {
                                doAckWsUpgrade();
                            } else {
                                reply_.stockReply(Reply::ok);
                                doWriteHeaders();
                            }
                        } else if (requestDecoder_.decodeRequest(request_, buffer_)) {
                            requestHandler_.handleRequest(connectionId_, request_, buffer_, reply_);
                            doWriteHeaders();
                        } else {
                            reply_.stockReply(Reply::bad_request);
                            doWriteHeaders();
                        }
                    } else if (result == RequestParser::good_part) {
                        if (requestDecoder_.decodeRequest(request_, buffer_)) {
                            reply_.noBodyBytesReceived_ = request_.getNoInitialBodyBytesReceived();
                            requestHandler_.handleRequest(connectionId_, request_, buffer_, reply_);
                            if (reply_.isMultiPart_) {
                                doWritePartAck();
                            } else {
                                doReadBody();
                            }
                        } else {
                            reply_.stockReply(Reply::bad_request);
                            doWriteHeaders();
                        }
                    } else if (result == RequestParser::bad) {
                        reply_.stockReply(Reply::bad_request);
                        doWriteHeaders();
                    } else {
                        doRead();
                    }
                }
            } else if (ec != asio::error::operation_aborted) {
                connectionManager_.debugMsg("doRead: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::doWritePartAck() {
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.headerToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                doReadBody();
            } else {
                connectionManager_.debugMsg("doWritePartAck: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                shutdown();
            }
        });
}

void Connection::doReadBody() {
    buffer_.resize(maxContentSize_);
    auto self(shared_from_this());
    socket_.async_read_some(
        asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
            if (!ec) {
                lastReceivedTime_ = std::chrono::steady_clock::now();
                buffer_.resize(bytesTransferred);
                reply_.noBodyBytesReceived_ += bytesTransferred;

                // As the receiving buffer is limited, keep track if we have
                // opened a new multi-part file and should send an ack or if we
                // just received file data for an already opened file.
                unsigned multiPartCounter = reply_.multiPartCounter_;

                requestHandler_.handlePartialWrite(connectionId_, request_, buffer_, reply_);
                if (reply_.noBodyBytesReceived_ < request_.contentLength_) {
                    if (multiPartCounter != reply_.multiPartCounter_) {
                        doWritePartAck();
                    } else {
                        doReadBody();
                    }
                } else {
                    doWriteHeaders();
                }
            } else if (ec != asio::error::operation_aborted) {
                connectionManager_.debugMsg("doReadBody: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::doWriteHeaders() {
    handleKeepAlive();
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.headerToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                if (!reply_.content_.empty() || reply_.contentPtr_ != nullptr) {
                    doWriteContent();
                } else {
                    handleWriteCompleted();
                }
            } else {
                connectionManager_.debugMsg("doWriteHeaders: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                shutdown();
            }
        });
}

void Connection::doAckWsUpgrade() {
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.headerToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                requestParser_.reset();
                request_.reset();
                reply_.reset();
                isWebSocket_ = true;
                wsHandler_.handleOnOpen(connectionId_);
                doRead();
            } else {
                connectionManager_.debugMsg("doAckWsUpgrade: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                shutdown();
            }
        });
}

void Connection::doWriteContent() {
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.contentToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                if (reply_.replyPartial_) {
                    if (reply_.finalPart_) {
                        handleWriteCompleted();
                    } else {
                        requestHandler_.handlePartialRead(connectionId_, request_, reply_);
                        doWriteContent();
                    }
                } else {
                    handleWriteCompleted();
                }
            } else {
                connectionManager_.debugMsg("doWriteContent: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                shutdown();
            }
        });
}

void Connection::handleKeepAlive() {
    nrOfRequest_++;
    if (useKeepAlive_ && request_.keepAlive_) {
        reply_.addHeader("Connection", "keep-alive");
        reply_.addHeader("Keep-Alive",
                         "timeout=" + std::to_string(keepAliveTimeout_.count()) +
                             ", max=" + std::to_string(keepAliveMax_));
    } else {
        reply_.addHeader("Connection", "close");
    }
}

void Connection::handleWriteCompleted() {
    if (useKeepAlive_ && request_.keepAlive_) {
        requestParser_.reset();
        request_.reset();
        reply_.reset();
        doRead();
    } else {
        // initiate graceful connection closure.
        std::error_code ignored_ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
        connectionManager_.stop(shared_from_this());
    }
}

bool Connection::handleUpgradeRequest() {
    std::string hVal = request_.getHeaderValue("Upgrade");
    if (strcasecmp(hVal.c_str(), "websocket") == 0) {
        reply_.addHeader("Connection", "Upgrade");
        reply_.addHeader("Upgrade", "websocket");
        std::string key = request_.getHeaderValue("Sec-WebSocket-Key");
        reply_.addHeader("Sec-Websocket-Accept", secAccept_.compute(key.data()));
        // At the moment no extensions are supported.
        // reply_.addHeader("Sec-WebSocket-Extensions", "permessage-deflate");
        reply_.send(Reply::switching_protocol);
        return true;
    }

    reply_.stockReply(Reply::ok);
    doWriteHeaders();
    return false;
}

void Connection::shutdown() {
    // initiate graceful connection closure.
    std::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
    connectionManager_.stop(shared_from_this());
    requestHandler_.closeFile(reply_, connectionId_);
}

}  // namespace beauty
