#pragma once
// included first
#include "beauty/environment.hpp"

#include <asio.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "beauty/header.hpp"
#include "beauty/i_http_client_handler.hpp"
#include "beauty/i_socket.hpp"
#include "beauty/response.hpp"
#include "beauty/response_parser.hpp"
#include "beauty/url_parser.hpp"

namespace beauty {

// An asynchronous HTTP/1.1 client that runs on a (shared) asio::io_context,
// following the same philosophy as the Beauty server and WebSocket client:
// fixed maximum size receive/send buffers, no per-message dynamic allocation of
// unbounded size, and fully asynchronous, callback based operation.
//
// The client is intended to be owned through a std::shared_ptr so that its
// lifetime is guaranteed for the duration of outstanding asynchronous
// operations. Use HttpClient::create() to construct one.
//
// Only a single request may be in flight at a time. Issue the next request from
// within the onResponse/onError callback (or after it returns).
class HttpClient : public std::enable_shared_from_this<HttpClient> {
   public:
    struct Config {
        // Maximum size of the receive buffer and of the accumulated response
        // body, in bytes. Responses whose body exceeds this are rejected.
        size_t maxResponseSize = 1024;

        // Maximum size of a request body, in bytes. Requests with a body
        // larger than this are rejected with an asynchronous onError.
        // Defaults to maxResponseSize.
        size_t maxRequestBodySize = 0;

        // Maximum time to wait for a request to complete (resolve + connect +
        // send + receive). 0 disables the timeout.
        std::chrono::milliseconds requestTimeout = std::chrono::milliseconds(5000);

        // Reuse the underlying TCP connection for subsequent requests to the
        // same host and port when the server allows it (keep-alive).
        bool keepAlive = true;
    };

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    static std::shared_ptr<HttpClient> create(asio::io_context& ioContext,
                                              IHttpClientHandler& handler,
                                              const Config& config);

    static std::shared_ptr<HttpClient> create(asio::io_context& ioContext,
                                              IHttpClientHandler& handler);

    // Create an HTTPS client. The caller must keep sslCtx alive for the
    // lifetime of the client.
    static std::shared_ptr<HttpClient> create(asio::io_context& ioContext,
                                              IHttpClientHandler& handler,
                                              const Config& config,
                                              std::shared_ptr<ISocket> socket);

    ~HttpClient() = default;

    // Perform an HTTP request against the given http:// URL. Returns false (and
    // does nothing) if a request is already in progress. On completion either
    // onResponse or onError is invoked exactly once.
    bool request(const std::string& method,
                 const std::string& url,
                 const std::vector<Header>& headers = std::vector<Header>(),
                 const std::string& body = std::string());

    // Convenience wrappers around request().
    bool get(const std::string& url, const std::vector<Header>& headers = std::vector<Header>());
    bool head(const std::string& url, const std::vector<Header>& headers = std::vector<Header>());
    bool del(const std::string& url, const std::vector<Header>& headers = std::vector<Header>());
    bool post(const std::string& url,
              const std::string& contentType,
              const std::string& body,
              const std::vector<Header>& headers = std::vector<Header>());
    bool put(const std::string& url,
             const std::string& contentType,
             const std::string& body,
             const std::vector<Header>& headers = std::vector<Header>());

    // Close the underlying connection (if any). Does not fire any callback.
    void close();

    // True while a request is in flight.
    bool busy() const {
        return requestInProgress_;
    }

   private:
    HttpClient(asio::io_context& ioContext,
               IHttpClientHandler& handler,
               const Config& config,
               std::shared_ptr<ISocket> socket);

    void startRequest();
    void doResolve();
    void doConnect(const asio::ip::tcp::resolver::results_type& endpoints);
    void doHandshake();
    void doWriteRequest();
    void doReadResponse();
    void startTimeoutTimer();

    void deliverResponse();
    void reportError(const std::string& error);
    void closeSocket();
    bool sameTarget(const std::string& host, const std::string& port) const;

    asio::io_context& ioContext_;
    asio::ip::tcp::resolver resolver_;
    std::shared_ptr<ISocket> socket_;
    asio::steady_timer timeoutTimer_;

    IHttpClientHandler& handler_;
    Config config_;

    // Fixed maximum size buffers.
    std::vector<char> recvBuffer_;   // Socket reads / response parsing input
    std::vector<char> sendBuffer_;   // Outgoing request body (bounded by maxRequestBodySize)
    std::vector<char> writeBuffer_;  // Combined headers + body for the wire
    std::vector<char> bodyBuffer_;   // Response body (referenced by response_)

    UrlParser url_;
    std::string host_;
    std::string port_;
    std::string method_;
    std::string headerBlock_;  // Rendered request-line + headers (no body)

    Response response_;
    ResponseParser responseParser_;

    bool requestInProgress_ = false;
    bool connected_ = false;
    std::string connectedHost_;
    std::string connectedPort_;
};

inline std::shared_ptr<HttpClient> HttpClient::create(asio::io_context& ioContext,
                                                      IHttpClientHandler& handler,
                                                      const Config& config) {
    return std::shared_ptr<HttpClient>(new HttpClient(
        ioContext, handler, config, std::shared_ptr<ISocket>(new PlainSocket(ioContext))));
}

inline std::shared_ptr<HttpClient> HttpClient::create(asio::io_context& ioContext,
                                                      IHttpClientHandler& handler) {
    return std::shared_ptr<HttpClient>(new HttpClient(
        ioContext, handler, Config(), std::shared_ptr<ISocket>(new PlainSocket(ioContext))));
}

inline std::shared_ptr<HttpClient> HttpClient::create(asio::io_context& ioContext,
                                                      IHttpClientHandler& handler,
                                                      const Config& config,
                                                      std::shared_ptr<ISocket> socket) {
    return std::shared_ptr<HttpClient>(new HttpClient(ioContext, handler, config, socket));
}

}  // namespace beauty
