#include <array>

#include "beauty/http_client.hpp"
#include "beauty/parse_common.hpp"

namespace beauty {

HttpClient::HttpClient(asio::io_context& ioContext,
                       IHttpClientHandler& handler,
                       const Config& config,
                       std::shared_ptr<ISocket> socket)
    : ioContext_(ioContext),
      resolver_(ioContext),
      socket_(socket),
      timeoutTimer_(ioContext),
      handler_(handler),
      config_(config),
      response_(bodyBuffer_),
      responseParser_() {
    if (!socket_) {
        socket_.reset(new PlainSocket(ioContext));
    }
    // Default maxRequestBodySize to maxResponseSize when left at 0.
    if (config_.maxRequestBodySize == 0) {
        config_.maxRequestBodySize = config_.maxResponseSize;
    }
    recvBuffer_.reserve(config_.maxResponseSize);
    sendBuffer_.reserve(config_.maxRequestBodySize);
    bodyBuffer_.reserve(config_.maxResponseSize);
    // Pre-allocate the write buffer for the maximum combined request size.
    // The 512-byte header budget covers the request line, Host, Connection,
    // Content-Length, and a reasonable set of user-supplied headers (the
    // reserve(256) in request() reflects the typical case).  If a request's
    // headers happen to exceed this, the vector simply grows once and stays
    // at the new high-water mark for subsequent requests.
    writeBuffer_.reserve(512 + config_.maxRequestBodySize);
    responseParser_.setMaxBodySize(config_.maxResponseSize);
}

bool HttpClient::request(const std::string& method,
                         const std::string& url,
                         const std::vector<Header>& headers,
                         const std::string& body) {
    if (requestInProgress_) {
        return false;
    }

    // Assume http:// when no scheme is provided.
    std::string effectiveUrl = url;
    if (url.find("://") == std::string::npos) {
        effectiveUrl = "http://" + url;
    }

    if (!url_.parse(effectiveUrl) || url_.hostname().empty()) {
        // Deliver the error asynchronously so the handler is never invoked
        // re-entrantly from request().
        requestInProgress_ = true;
        auto self(shared_from_this());
        asio::post(ioContext_, [this, self]() { reportError("Invalid URL"); });
        return true;
    }

    host_ = url_.hostname();
    port_ = std::to_string(url_.httpPort());
    method_ = method;

    // Reject bodies that exceed the configured cap.
    if (body.size() > config_.maxRequestBodySize) {
        requestInProgress_ = true;
        auto self(shared_from_this());
        asio::post(ioContext_,
                   [this, self]() { reportError("Request body exceeds maximum size"); });
        return true;
    }

    std::string target = url_.path();
    if (target.empty()) {
        target = "/";
    }
    if (!url_.query().empty()) {
        target += "?" + url_.query();
    }

    // Build only headers into headerBlock_ (bounded, small).
    std::string req;
    req.reserve(256);
    req += method_ + " " + target + " HTTP/1.1\r\n";
    req += "Host: " + host_ + ":" + port_ + "\r\n";
    req += std::string("Connection: ") + (config_.keepAlive ? "keep-alive" : "close") + "\r\n";
    for (const Header& h : headers) {
        req += h.name_ + ": " + h.value_ + "\r\n";
    }
    if (!body.empty()) {
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += "\r\n";

    headerBlock_ = std::move(req);

    // Store body separately in the fixed-size send buffer.
    sendBuffer_.assign(body.begin(), body.end());

    startRequest();
    return true;
}

bool HttpClient::get(const std::string& url, const std::vector<Header>& headers) {
    return request("GET", url, headers, std::string());
}

bool HttpClient::head(const std::string& url, const std::vector<Header>& headers) {
    return request("HEAD", url, headers, std::string());
}

bool HttpClient::del(const std::string& url, const std::vector<Header>& headers) {
    return request("DELETE", url, headers, std::string());
}

bool HttpClient::post(const std::string& url,
                      const std::string& contentType,
                      const std::string& body,
                      const std::vector<Header>& headers) {
    std::vector<Header> all = headers;
    all.push_back(Header{"Content-Type", contentType});
    return request("POST", url, all, body);
}

bool HttpClient::put(const std::string& url,
                     const std::string& contentType,
                     const std::string& body,
                     const std::vector<Header>& headers) {
    std::vector<Header> all = headers;
    all.push_back(Header{"Content-Type", contentType});
    return request("PUT", url, all, body);
}

void HttpClient::startRequest() {
    requestInProgress_ = true;
    responseParser_.reset();
    responseParser_.setHeadRequest(method_ == "HEAD");
    response_.reset();
    recvBuffer_.clear();
    bodyBuffer_.clear();

    startTimeoutTimer();

    // Reuse an existing keep-alive connection to the same target if possible.
    if (connected_ && socket_->isOpen() && sameTarget(host_, port_)) {
        doWriteRequest();
    } else {
        closeSocket();
        doResolve();
    }
}

void HttpClient::startTimeoutTimer() {
    if (config_.requestTimeout.count() == 0) {
        return;
    }
    auto self(shared_from_this());
    timeoutTimer_.expires_after(config_.requestTimeout);
    timeoutTimer_.async_wait([this, self](const std::error_code& ec) {
        if (!ec && requestInProgress_) {
            reportError("Request timed out");
        }
    });
}

void HttpClient::doResolve() {
    auto self(shared_from_this());
    resolver_.async_resolve(host_,
                            port_,
                            [this, self](const std::error_code& ec,
                                         const asio::ip::tcp::resolver::results_type& endpoints) {
                                if (!requestInProgress_) {
                                    return;
                                }
                                if (ec) {
                                    reportError("Resolve failed: " + ec.message());
                                    return;
                                }
                                doConnect(endpoints);
                            });
}

void HttpClient::doConnect(const asio::ip::tcp::resolver::results_type& endpoints) {
    auto self(shared_from_this());
    socket_->asyncConnect(endpoints,
                          [this, self](const std::error_code& ec, const asio::ip::tcp::endpoint&) {
                              if (!requestInProgress_) {
                                  return;
                              }
                              if (ec) {
                                  reportError("Connect failed: " + ec.message());
                                  return;
                              }
                              connected_ = true;
                              connectedHost_ = host_;
                              connectedPort_ = port_;
                              doHandshake();
                          });
}

void HttpClient::doHandshake() {
    auto self(shared_from_this());
    socket_->asyncHandshake(false, [this, self](const std::error_code& ec) {
        if (!requestInProgress_) {
            return;
        }
        if (ec) {
            reportError("TLS handshake failed: " + ec.message());
            return;
        }
        doWriteRequest();
    });
}

void HttpClient::doWriteRequest() {
    // Build a single contiguous write buffer from headerBlock_ + sendBuffer_.
    // Scatter-gather writes (writev/sendmsg) are not reliably supported on all
    // platforms (e.g. lwIP on ESP-IDF).  writeBuffer_ is pre-reserved in the
    // constructor so this copy does not allocate in the common case.
    writeBuffer_.clear();
    writeBuffer_.insert(writeBuffer_.end(), headerBlock_.begin(), headerBlock_.end());
    writeBuffer_.insert(writeBuffer_.end(), sendBuffer_.begin(), sendBuffer_.end());

    auto self(shared_from_this());
    socket_->asyncWrite({asio::buffer(writeBuffer_)},
                        [this, self](const std::error_code& ec, std::size_t) {
                            if (ec) {
                                // A keep-alive connection may have been closed by the server
                                // between requests; surface it as an error.
                                reportError("Write failed: " + ec.message());
                                return;
                            }
                            doReadResponse();
                        });
}

void HttpClient::doReadResponse() {
    auto self(shared_from_this());
    recvBuffer_.resize(config_.maxResponseSize);
    socket_->asyncReadSome(
        asio::buffer(recvBuffer_),
        [this, self](const std::error_code& ec, std::size_t bytesTransferred) {
            if (ec) {
                if (ec == asio::error::eof || ec == asio::error::connection_reset) {
                    // The server closed the connection. If the body was
                    // delimited by connection close this completes the response.
                    connected_ = false;
                    ResponseParser::result_type result = responseParser_.finish(response_);
                    if (result == ResponseParser::good_complete) {
                        deliverResponse();
                    } else {
                        reportError("Connection closed before response completed");
                    }
                } else {
                    reportError("Read failed: " + ec.message());
                }
                return;
            }

            recvBuffer_.resize(bytesTransferred);
            ResponseParser::result_type result = responseParser_.parse(response_, recvBuffer_);

            if (result == ResponseParser::good_complete) {
                deliverResponse();
            } else if (result == ResponseParser::good_part) {
                doReadResponse();
            } else if (result == ResponseParser::too_large) {
                reportError("Response body exceeds maximum size");
            } else if (result == ResponseParser::version_not_supported) {
                reportError("Unsupported HTTP version in response");
            } else if (result == ResponseParser::switching_protocols) {
                reportError("Unexpected 101 Switching Protocols response");
            } else {
                reportError("Malformed HTTP response");
            }
        });
}

void HttpClient::deliverResponse() {
    timeoutTimer_.cancel();
    requestInProgress_ = false;

    bool keep = config_.keepAlive && response_.keepAlive_ && connected_;
    if (!keep) {
        closeSocket();
    }

    // Snapshot the response into stack-locals so that a re-entrant
    // request() from within onResponse does not mutate the object the
    // handler is still inspecting.
    std::vector<char> deliveredBody;
    deliveredBody.swap(bodyBuffer_);
    Response snapshot(deliveredBody);
    snapshot.httpVersionMajor_ = response_.httpVersionMajor_;
    snapshot.httpVersionMinor_ = response_.httpVersionMinor_;
    snapshot.statusCode_ = response_.statusCode_;
    snapshot.statusMessage_.swap(response_.statusMessage_);
    snapshot.headers_.swap(response_.headers_);
    snapshot.keepAlive_ = response_.keepAlive_;

    handler_.onResponse(snapshot);
}

void HttpClient::reportError(const std::string& error) {
    if (!requestInProgress_) {
        return;
    }
    timeoutTimer_.cancel();
    requestInProgress_ = false;
    closeSocket();
    handler_.onError(error);
}

void HttpClient::close() {
    timeoutTimer_.cancel();
    resolver_.cancel();
    requestInProgress_ = false;
    closeSocket();
}

void HttpClient::closeSocket() {
    resolver_.cancel();
    socket_->close();
    connected_ = false;
    connectedHost_.clear();
    connectedPort_.clear();
}

bool HttpClient::sameTarget(const std::string& host, const std::string& port) const {
    return connectedHost_ == host && connectedPort_ == port;
}

}  // namespace beauty
