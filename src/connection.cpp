#include "beauty/multipart_parser.hpp"
#include "beauty/ws_sec_accept.hpp"
#include "beauty/connection_manager.hpp"
#include "beauty/connection.hpp"

namespace beauty {

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionManager &manager,
                       RequestHandler &handler,
                       IWsReceiver *wsReceiver,
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
      wsReceiver_(wsReceiver),
      wsMessage_(recvBuffer_),
      wsParser_(wsMessage_) {}

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

size_t Connection::getNrOfRequests() const {
    return nrOfRequest_;
}

bool Connection::useKeepAlive() const {
    return (useKeepAlive_ && request_.keepAlive_);
}

bool Connection::isWebSocket() const {
    return isWebSocket_;
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
                    // TODO: do something with the data
                    if (result == WsParser::indeterminate || result == WsParser::data_frame) {
                        lastReceivedTime_ = lastActivityTime_;
                        wsReceiver_->onWsMessage(std::to_string(connectionId_), wsMessage_);
                        doRead();
                    } else if (result == WsParser::close_frame) {
                        // Client closed the connection
                        wsReceiver_->onWsClose(std::to_string(connectionId_));
                        connectionManager_.stop(shared_from_this());
                    } else if (result == WsParser::ping_frame) {
                        // Respond with pong
                        lastReceivedTime_ = lastActivityTime_;
                        // sendWsPong(wsMessage_.content_);
                        // doWriteContent();
                    } else if (result == WsParser::pong_frame) {
                        // Pong received - can update ping status
                        doRead();
                    } else if (result == WsParser::fragmentation_error) {
                        // Fragmented messages are not supported
                        // send some error close frame?
                        // connectionManager_.stop(shared_from_this());
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
                        if (wsReceiver_ == nullptr) {
                            // No WebSocket receiver available
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

void Connection::doWriteContent() {
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.contentToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                lastActivityTime_ = std::chrono::steady_clock::now();

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

void Connection::handleConnection() {
    nrOfRequest_++;

    // Check if server wants to close the connection
    auto it = std::find_if(reply_.headers_.begin(), reply_.headers_.end(), [](const Header &h) {
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
                wsReceiver_->onWsOpen(std::to_string(connectionId_));
                doRead();
            } else {
                connectionManager_.debugMsg("doAckWsUpgrade: " + ec.message() + ':' +
                                            std::to_string(ec.value()));
                shutdown();
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
