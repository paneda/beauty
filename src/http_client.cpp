#include "beauty/http_client.hpp"

#include "beauty/parse_common.hpp"

namespace beauty {

HttpClient::HttpClient(asio::io_context &ioContext,
                       IHttpClientHandler &handler,
                       const Config &config)
    : ioContext_(ioContext),
      resolver_(ioContext),
      socket_(ioContext),
      timeoutTimer_(ioContext),
      handler_(handler),
      config_(config),
      response_(bodyBuffer_),
      responseParser_() {
    recvBuffer_.reserve(config_.maxResponseSize);
    bodyBuffer_.reserve(config_.maxResponseSize);
    responseParser_.setMaxBodySize(config_.maxResponseSize);
}

bool HttpClient::request(const std::string &method,
                         const std::string &url,
                         const std::vector<Header> &headers,
                         const std::string &body) {
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

    std::string target = url_.path();
    if (target.empty()) {
        target = "/";
    }
    if (!url_.query().empty()) {
        target += "?" + url_.query();
    }

    // Render the request head (and body) into a single buffer.
    std::string req;
    req.reserve(128 + body.size());
    req += method_ + " " + target + " HTTP/1.1\r\n";
    req += "Host: " + host_ + ":" + port_ + "\r\n";
    req += std::string("Connection: ") + (config_.keepAlive ? "keep-alive" : "close") + "\r\n";
    for (const Header &h : headers) {
        req += h.name_ + ": " + h.value_ + "\r\n";
    }
    if (!body.empty()) {
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += "\r\n";
    req += body;

    headerBlock_ = std::move(req);

    startRequest();
    return true;
}

bool HttpClient::get(const std::string &url, const std::vector<Header> &headers) {
    return request("GET", url, headers, std::string());
}

bool HttpClient::head(const std::string &url, const std::vector<Header> &headers) {
    return request("HEAD", url, headers, std::string());
}

bool HttpClient::del(const std::string &url, const std::vector<Header> &headers) {
    return request("DELETE", url, headers, std::string());
}

bool HttpClient::post(const std::string &url,
                      const std::string &contentType,
                      const std::string &body,
                      const std::vector<Header> &headers) {
    std::vector<Header> all = headers;
    all.push_back(Header{"Content-Type", contentType});
    return request("POST", url, all, body);
}

bool HttpClient::put(const std::string &url,
                     const std::string &contentType,
                     const std::string &body,
                     const std::vector<Header> &headers) {
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
    if (connected_ && socket_.is_open() && sameTarget(host_, port_)) {
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
    timeoutTimer_.async_wait([this, self](const std::error_code &ec) {
        if (!ec && requestInProgress_) {
            reportError("Request timed out");
        }
    });
}

void HttpClient::doResolve() {
    auto self(shared_from_this());
    resolver_.async_resolve(host_,
                            port_,
                            [this, self](const std::error_code &ec,
                                         const asio::ip::tcp::resolver::results_type &endpoints) {
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

void HttpClient::doConnect(const asio::ip::tcp::resolver::results_type &endpoints) {
    auto self(shared_from_this());
    asio::async_connect(socket_,
                        endpoints,
                        [this, self](const std::error_code &ec, const asio::ip::tcp::endpoint &) {
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
                            doWriteRequest();
                        });
}

void HttpClient::doWriteRequest() {
    sendBuffer_.assign(headerBlock_.begin(), headerBlock_.end());
    auto self(shared_from_this());
    asio::async_write(
        socket_, asio::buffer(sendBuffer_), [this, self](const std::error_code &ec, std::size_t) {
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
    socket_.async_read_some(
        asio::buffer(recvBuffer_),
        [this, self](const std::error_code &ec, std::size_t bytesTransferred) {
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

    handler_.onResponse(response_);
}

void HttpClient::reportError(const std::string &error) {
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
    std::error_code ignore;
    resolver_.cancel();
    socket_.close(ignore);
    connected_ = false;
    connectedHost_.clear();
    connectedPort_.clear();
}

bool HttpClient::sameTarget(const std::string &host, const std::string &port) const {
    return connectedHost_ == host && connectedPort_ == port;
}

}  // namespace beauty
