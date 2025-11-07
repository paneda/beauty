#include <iostream>

#include "beauty/multipart_parser.hpp"
#include "beauty/ws_sec_accept.hpp"
#include "beauty/connection_manager.hpp"
#include "beauty/connection.hpp"
#include "beauty/ws_endpoint.hpp"

namespace beauty {

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionManager& manager,
                       RequestHandler& handler,
                       unsigned connectionId,
                       size_t maxContentSize)
    : socket_(std::move(socket)),
      connectionManager_(manager),
      requestHandler_(handler),
      connectionId_(connectionId),
      maxContentSize_(maxContentSize),
      recvBuffer_(maxContentSize),
      sendBuffer_(maxContentSize),
      request_(recvBuffer_),
      reply_(sendBuffer_),
      wsEncoder_(sendBuffer_),
      wsMessage_(recvBuffer_),
      wsParser_(wsMessage_),
      writeInProgress_(false) {}

void Connection::start(bool useKeepAlive,
                       std::chrono::seconds keepAliveTimeout,
                       size_t keepAliveMax) {
    lastActivityTime_ = std::chrono::steady_clock::now();
    lastReceivedTime_ = lastActivityTime_;
    useKeepAlive_ = useKeepAlive;
    keepAliveTimeout_ = keepAliveTimeout;
    keepAliveMax_ = keepAliveMax;
    doRead();
}

void Connection::stop() {
    socket_.close();
}

std::chrono::steady_clock::time_point Connection::getLastActivityTime() const {
    return lastActivityTime_;
}

std::chrono::steady_clock::time_point Connection::getLastReceivedTime() const {
    return lastReceivedTime_;
}

std::chrono::steady_clock::time_point Connection::getLastPingTime() const {
    return lastPingTime_;
}

std::chrono::steady_clock::time_point Connection::getLastPongTime() const {
    return lastPongTime_;
}

size_t Connection::getNrOfRequests() const {
    return nrOfRequest_;
}

bool Connection::useKeepAlive() const {
    return (useKeepAlive_ && request_.keepAlive_);
}

bool Connection::isWebSocket() const {
    return isWebSocket_;
}

unsigned Connection::getConnectionId() const {
    return connectionId_;
}

WsEndpoint* Connection::getWsEndpoint() const {
    return wsEndpoint_;
}

bool Connection::isWriteInProgress() const {
    return writeInProgress_;
}

void Connection::sendWsPing() {
    if (!isWebSocket_) {
        return;
    }
    wsEncoder_.encodePingFrame();
    lastPingTime_ = std::chrono::steady_clock::now();
    doWriteWsFrame(false, nullptr);
}

WriteResult Connection::sendWsText(const std::string& message, WriteCompleteCallback callback) {
    if (!isWebSocket_) {
        return WriteResult::CONNECTION_CLOSED;
    }
    if (writeInProgress_) {
        return WriteResult::WRITE_IN_PROGRESS;
    }

    wsEncoder_.encodeTextFrame(message);
    doWriteWsFrame(false, callback);
    return WriteResult::SUCCESS;
}

WriteResult Connection::sendWsBinary(const std::vector<char>& data,
                                     WriteCompleteCallback callback) {
    if (!isWebSocket_) {
        return WriteResult::CONNECTION_CLOSED;
    }
    if (writeInProgress_) {
        return WriteResult::WRITE_IN_PROGRESS;
    }

    wsEncoder_.encodeBinaryFrame(data);
    doWriteWsFrame(false, callback);
    return WriteResult::SUCCESS;
}

WriteResult Connection::sendWsClose(uint16_t statusCode,
                                    const std::string& reason,
                                    WriteCompleteCallback callback) {
    if (!isWebSocket_) {
        return WriteResult::CONNECTION_CLOSED;
    }
    if (writeInProgress_) {
        return WriteResult::WRITE_IN_PROGRESS;
    }

    wsEncoder_.encodeCloseFrame(statusCode, reason);
    doWriteWsFrame(false, callback);
    return WriteResult::SUCCESS;
}

void Connection::doRead() {
    auto self(shared_from_this());
    // Asio uses recvBuffer_.size() to limit amount of read data so must restore
    // size before reading. Note: operation is "cheap" as maxContentSize is
    // already reserved.
    recvBuffer_.resize(maxContentSize_);
    socket_.async_read_some(
        asio::buffer(recvBuffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
            if (!ec) {
                lastActivityTime_ = std::chrono::steady_clock::now();
                recvBuffer_.resize(bytesTransferred);
                if (isWebSocket_) {
                    wsMessage_.reset();
                    WsParser::result_type result = wsParser_.parse();
                    if (result == WsParser::indeterminate || result == WsParser::data_frame) {
                        lastReceivedTime_ = lastActivityTime_;
                        if (wsEndpoint_) {
                            wsEndpoint_->onWsMessage(std::to_string(connectionId_), wsMessage_);
                        }
                        doRead();
                    } else if (result == WsParser::close_frame) {
                        // Client closed the connection
                        if (wsEndpoint_) {
                            wsEndpoint_->onWsClose(std::to_string(connectionId_));
                        }
                        connectionManager_.stop(shared_from_this());
                    } else if (result == WsParser::ping_frame) {
                        // Respond with pong
                        lastReceivedTime_ = lastActivityTime_;
                        wsEncoder_.encodePongFrame(wsMessage_.content_);
                        doWriteWsFrame(true, nullptr);  // Continue reading after pong is sent
                    } else if (result == WsParser::pong_frame) {
                        lastPongTime_ = lastActivityTime_;
                        doRead();
                    } else if (result == WsParser::fragmentation_error) {
                        // Fragmented messages are not supported
                        if (wsEndpoint_) {
                            wsEndpoint_->onWsError(std::to_string(connectionId_),
                                                   "Fragmented messages are not supported");
                        }
                        // Send close frame and stop connection
                        wsEncoder_.encodeCloseFrame(1003, "Fragmented messages not supported");
                        doWriteWsFrame(false, nullptr);
                        connectionManager_.stop(shared_from_this());
                    }
                } else {
                    RequestParser::result_type result = requestParser_.parse(request_, recvBuffer_);

                    if (result == RequestParser::good_complete) {
                        if (requestDecoder_.decodeRequest(request_, recvBuffer_)) {
                            requestHandler_.handleRequest(
                                connectionId_, request_, recvBuffer_, reply_);
                            doWriteHeaders();
                        } else {
                            reply_.stockReply(request_, Reply::bad_request);
                            doWriteHeaders();
                        }
                    } else if (result == RequestParser::good_headers_expect_continue) {
                        if (requestDecoder_.decodeRequest(request_, recvBuffer_)) {
                            if (request_.contentLength_ > maxContentSize_) {
                                bool isMultipart = MultiPartParser::isMultipartRequest(request_);

                                if (!isMultipart) {
                                    // By design Beauty only supports large body data
                                    // uploads using multipart/form-data. It will not
                                    // allocate buffer > maxContentSize_ for non-multipart data
                                    reply_.stockReply(request_, Reply::payload_too_large);
                                    doWriteHeaders();
                                    return;
                                }
                            }

                            // Check if the application wants to continue with this request
                            requestHandler_.shouldContinueAfterHeaders(request_, reply_);
                            if (reply_.isStatusOk()) {
                                doWrite100Continue();
                            } else {
                                // Application rejected the request, send its reply
                                reply_.addHeader("Connection", "close");
                                doWriteHeaders();
                            }
                        } else {
                            reply_.stockReply(request_, Reply::bad_request);
                            doWriteHeaders();
                        }
                    } else if (result == RequestParser::expect_continue_with_body) {
                        // Parser detected 100-continue protocol violation: client sent Expect
                        // header with body data without waiting for 100 Continue response
                        reply_.stockReply(request_, Reply::expectation_failed);
                        doWriteHeaders();
                    } else if (result == RequestParser::good_part) {
                        // Determine if this is multipart without processing the request yet
                        // (since we have incomplete body data)
                        bool isMultipart = MultiPartParser::isMultipartRequest(request_);
                        if (!isMultipart) {
                            if (request_.contentLength_ > maxContentSize_) {
                                // By design Beauty only supports large body data
                                // uploads using multipart/form-data. It will not
                                // allocate buffer > maxContentSize_ for non-multipart data
                                reply_.stockReply(request_, Reply::payload_too_large);
                                doWriteHeaders();
                                return;
                            } else if (request_.contentLength_ > 0) {
                                // If we haven't received all body bytes yet, but expect some,
                                // we need to wait for more data
                                doRead();
                                return;
                            }
                        }

                        if (requestDecoder_.decodeRequest(request_, recvBuffer_)) {
                            reply_.noBodyBytesReceived_ = request_.getNoInitialBodyBytesReceived();

                            // Call handleRequest normally - it will set up multipart state and
                            // process initial chunk
                            requestHandler_.handleRequest(
                                connectionId_, request_, recvBuffer_, reply_);
                            // Provide an early response to client if an error occurred
                            if (!reply_.isStatusOk()) {
                                reply_.addHeader("Connection", "close");
                                doWriteHeaders();
                                return;
                            }

                            doReadBody();
                        } else {
                            reply_.stockReply(request_, Reply::bad_request);
                            doWriteHeaders();
                        }
                    } else if (result == RequestParser::upgrade_to_websocket) {
                        requestDecoder_.decodeRequest(request_, recvBuffer_);
                        // Look up WebSocket endpoint based on requested path
                        wsEndpoint_ =
                            connectionManager_.getWsEndpointForPath(request_.requestPath_);
                        if (wsEndpoint_ == nullptr) {
                            // No WebSocket endpoint available for this path
                            reply_.stockReply(request_, Reply::bad_request);
                            doWriteHeaders();
                            return;
                        }
                        handleUpgradeToWebSocket();
                    } else if (result == RequestParser::missing_content_length) {
                        reply_.stockReply(request_, Reply::length_required);
                        doWriteHeaders();
                    } else if (result == RequestParser::version_not_supported) {
                        reply_.stockReply(request_, Reply::status_type::version_not_supported);
                        doWriteHeaders();
                    } else if (result == RequestParser::bad) {
                        reply_.stockReply(request_, Reply::bad_request);
                        doWriteHeaders();
                    } else {
                        doRead();
                    }
                }
            } else if (ec != asio::error::operation_aborted) {
                connectionManager_.debugMsg("doRead: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                // Notify WebSocket endpoint of error if this is a WebSocket connection
                if (isWebSocket_ && wsEndpoint_) {
                    wsEndpoint_->onWsError(std::to_string(connectionId_),
                                           "Read error: " + ec.message());
                }
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::doReadBody() {
    recvBuffer_.resize(maxContentSize_);
    auto self(shared_from_this());
    socket_.async_read_some(
        asio::buffer(recvBuffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
            if (!ec) {
                lastActivityTime_ = std::chrono::steady_clock::now();
                lastReceivedTime_ = lastActivityTime_;
                recvBuffer_.resize(bytesTransferred);
                reply_.noBodyBytesReceived_ += bytesTransferred;

                // Process more body data using handlePartialWrite as this is the only
                // supported mode to handle additional body data after initial
                // request processing.
                requestHandler_.handlePartialWrite(connectionId_, request_, recvBuffer_, reply_);

                if (reply_.noBodyBytesReceived_ < request_.contentLength_) {
                    // Provide an early response to client if an error occurred
                    if (!reply_.isStatusOk()) {
                        reply_.addHeader("Connection", "close");
                        doWriteHeaders();
                        return;
                    }
                    doReadBody();
                } else {
                    // Body complete, send final response
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
    handleConnection();
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.headerToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                lastActivityTime_ = std::chrono::steady_clock::now();

                if (!reply_.content_.empty() || reply_.contentPtr_ != nullptr) {
                    doWriteReplyContent();
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

void Connection::doWriteReplyContent() {
    auto self(shared_from_this());

    // Handle big data callback before writing
    if (reply_.streamCallback_ && !reply_.finalPart_) {
        reply_.content_.resize(maxContentSize_);
        int bytesRead = reply_.streamCallback_(
            std::to_string(connectionId_), reply_.content_.data(), maxContentSize_);

        if (bytesRead > 0) {
            if (static_cast<size_t>(bytesRead) > maxContentSize_) {
                // Callback returned invalid size, this is en error made by the
                // application. Log the error and shutdown the connection.
                reply_.finalPart_ = true;
                reply_.content_.clear();
                connectionManager_.debugMsg(
                    "doWriteReplyContent: stream callback returned invalid size");
                shutdown();
            } else {
                reply_.content_.resize(bytesRead);
                reply_.streamedBytes_ += bytesRead;
                reply_.finalPart_ = (reply_.streamedBytes_ >= reply_.totalStreamSize_) ||
                                    (bytesRead < static_cast<int>(maxContentSize_));
            }
        } else {
            reply_.finalPart_ = true;
            reply_.content_.clear();
        }
    }

    asio::async_write(
        socket_, reply_.contentToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                lastActivityTime_ = std::chrono::steady_clock::now();

                if (reply_.replyPartial_) {
                    if (reply_.finalPart_) {
                        handleWriteCompleted();
                    } else {
                        if (!reply_.streamCallback_) {
                            // FileIO streaming
                            requestHandler_.handlePartialRead(connectionId_, request_, reply_);
                        }
                        doWriteReplyContent();
                    }
                } else {
                    handleWriteCompleted();
                }
            } else {
                connectionManager_.debugMsg("doWriteReplyContent: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                shutdown();
            }
        });
}

void Connection::handleConnection() {
    nrOfRequest_++;

    // Check if server wants to close the connection
    auto it = std::find_if(reply_.headers_.begin(), reply_.headers_.end(), [](const Header& h) {
        return strcasecmp(h.name_.c_str(), "Connection") == 0 &&
               strcasecmp(h.value_.c_str(), "close") == 0;
    });
    if (it != reply_.headers_.end()) {
        closeConnection_ = true;
        return;
    }

    // Check if client wants to close the connection
    if (request_.keepAlive_ == false) {
        reply_.addHeader("Connection", "close");
        closeConnection_ = true;
        return;
    }

    // Check if we should use keep-alive
    if (useKeepAlive_) {
        reply_.addHeader("Connection", "keep-alive");
        reply_.addHeader("Keep-Alive",
                         "timeout=" + std::to_string(keepAliveTimeout_.count()) +
                             ", max=" + std::to_string(keepAliveMax_));
        return;
    }

    // Default in HTTP/1.1 is keep-alive, but if server does not want to use
    // it, we must close the connection here
    reply_.addHeader("Connection", "close");
    closeConnection_ = true;
}

void Connection::handleWriteCompleted() {
    requestParser_.reset();
    request_.reset();
    reply_.reset();
    firstBodyReadAfter100Continue_ = true;  // Reset for next request

    if (!closeConnection_) {
        doRead();
    } else {
        // Initiate graceful connection closure
        std::error_code ignored_ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
        connectionManager_.stop(shared_from_this());
    }
}

void Connection::doWrite100Continue() {
    auto self(shared_from_this());

    // Create 100 Continue response
    std::string continueResponse = "HTTP/1.1 100 Continue\r\n\r\n";

    asio::async_write(
        socket_, asio::buffer(continueResponse), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                // Initialize reply for body reading - no body bytes received yet
                reply_.noBodyBytesReceived_ = 0;
                // Now read the body using special method for 100-continue
                doReadBodyAfter100Continue();
            } else {
                connectionManager_.debugMsg("doWrite100Continue: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                shutdown();
            }
        });
}

void Connection::doReadBodyAfter100Continue() {
    recvBuffer_.resize(maxContentSize_);
    auto self(shared_from_this());
    socket_.async_read_some(
        asio::buffer(recvBuffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
            if (!ec) {
                lastActivityTime_ = std::chrono::steady_clock::now();
                lastReceivedTime_ = lastActivityTime_;
                recvBuffer_.resize(bytesTransferred);
                reply_.noBodyBytesReceived_ += bytesTransferred;

                if (firstBodyReadAfter100Continue_) {
                    firstBodyReadAfter100Continue_ = false;

                    // Clear 100-continue response data
                    reply_.headers_.clear();
                    reply_.returnToClient_ = false;
                    reply_.status_ = Reply::status_type::ok;
                    reply_.content_.clear();
                    reply_.contentPtr_ = nullptr;
                    reply_.contentSize_ = 0;

                    // handleRequest needs to be called first time to handle
                    // either "single part" or multi-part body processing
                    requestHandler_.handleRequest(connectionId_, request_, recvBuffer_, reply_);
                    if (!reply_.isMultiPart_) {
                        if (!reply_.isStatusOk()) {
                            reply_.addHeader("Connection", "close");
                        }
                        doWriteHeaders();
                        return;
                    }
                }

                if (reply_.noBodyBytesReceived_ < request_.contentLength_) {
                    // Body is incomplete, continue reading
                    // Always use handlePartialWrite for body processing (multipart state already
                    // set up)
                    requestHandler_.handlePartialWrite(
                        connectionId_, request_, recvBuffer_, reply_);
                    if (!reply_.isStatusOk()) {
                        reply_.addHeader("Connection", "close");
                        doWriteHeaders();
                        return;
                    }
                    doReadBodyAfter100Continue();
                } else {
                    requestHandler_.handlePartialWrite(
                        connectionId_, request_, recvBuffer_, reply_);
                    doWriteHeaders();
                }
            } else if (ec != asio::error::operation_aborted) {
                connectionManager_.debugMsg("doReadBodyAfter100Continue: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::handleUpgradeToWebSocket() {
    reply_.addHeader("Connection", "Upgrade");
    reply_.addHeader("Upgrade", "websocket");
    std::string key = request_.getHeaderValue("Sec-WebSocket-Key");
    if (key.empty()) {
        reply_.stockReply(request_, Reply::bad_request);
        reply_.addHeader("Connection", "close");
        doWriteHeaders();
        return;
    }
    reply_.addHeader("Sec-Websocket-Accept", computeWsSecAccept(key.data()));
    // At the moment no extensions are supported.
    // reply_.addHeader("Sec-WebSocket-Extensions", "permessage-deflate");
    reply_.send(Reply::switching_protocols);

    doAckWsUpgrade();
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
                connectionManager_.debugMsg("WebSocket upgraded on path: " + request_.requestPath_);
                if (wsEndpoint_) {
                    wsEndpoint_->onWsOpen(std::to_string(connectionId_));
                }
                doRead();
            } else {
                connectionManager_.debugMsg("doAckWsUpgrade: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                // Notify WebSocket endpoint of upgrade failure
                if (wsEndpoint_) {
                    wsEndpoint_->onWsError(std::to_string(connectionId_),
                                           "WebSocket upgrade failed: " + ec.message());
                }
                shutdown();
            }
        });
}

void Connection::doWriteWsFrame(bool continueReading, WriteCompleteCallback callback) {
    writeInProgress_ = true;
    writeCallback_ = callback;

    auto self(shared_from_this());
    std::vector<asio::const_buffer> buffers;
    buffers.push_back(asio::buffer(sendBuffer_));
    asio::async_write(socket_,
                      buffers,
                      [this, self, continueReading](std::error_code ec, std::size_t bytesWritten) {
                          writeInProgress_ = false;

                          if (!ec) {
                              lastActivityTime_ = std::chrono::steady_clock::now();
                              if (continueReading) {
                                  doRead();  // Continue reading after write completes
                              }
                          } else {
                              connectionManager_.debugMsg("doWriteWsFrame: " + ec.message() + ':' +
                                                          std::to_string(ec.value()));
                              // Notify WebSocket endpoint of write error
                              if (wsEndpoint_) {
                                  wsEndpoint_->onWsError(std::to_string(connectionId_),
                                                         "Write error: " + ec.message());
                              }
                              shutdown();
                          }

                          // Call completion callback if provided
                          if (writeCallback_) {
                              writeCallback_(ec, bytesWritten);
                              writeCallback_ = nullptr;
                          }
                      });
}

void Connection::shutdown() {
    // Initiate graceful connection closure
    std::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
    connectionManager_.stop(shared_from_this());
    requestHandler_.closeFile(connectionId_);
}

}  // namespace beauty
