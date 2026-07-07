#include <signal.h>
#include <chrono>
#include <utility>

#include "beauty/server.hpp"
#include "beauty/i_socket.hpp"
#include "beauty/ws_endpoint.hpp"

// SslSocket requires <asio/ssl.hpp> which is only available when SSL support
// is enabled. On ESP-IDF this is gated by CONFIG_ASIO_SSL_SUPPORT.
#if defined(CONFIG_ASIO_SSL_SUPPORT) || defined(BEAUTY_ENABLE_SSL) || defined(ASIO_SSL_HPP)
#include "beauty/ssl_socket.hpp"
#define BEAUTY_HAS_SSL 1
#endif

namespace {
void defaultDebugMsgHandler(const std::string &) {}
}

namespace beauty {

Server::Server(asio::io_context &ioContext,
               uint16_t port,
               const Settings &settings,
               size_t maxContentSize)
    : ioContext_(ioContext),
      acceptor_(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      connectionManager_(settings),
      requestHandler_(maxContentSize),
      timer_(ioContext),
      maxContentSize_(maxContentSize),
      debugMsgCb_(defaultDebugMsgHandler) {
    if (maxContentSize < 1024) {
        debugMsgCb_("maxContentSize must be equal or larger than 1024 bytes");
        return;
    }
    doAccept();
    doTick();
}

Server::Server(asio::io_context &ioContext,
               uint16_t port,
               asio::ssl::context &sslCtx,
               const Settings &settings,
               size_t maxContentSize)
    : ioContext_(ioContext),
      acceptor_(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      sslCtx_(&sslCtx),
      connectionManager_(settings),
      requestHandler_(maxContentSize),
      timer_(ioContext),
      maxContentSize_(maxContentSize),
      debugMsgCb_(defaultDebugMsgHandler) {
    if (maxContentSize < 1024) {
        debugMsgCb_("maxContentSize must be equal or larger than 1024 bytes");
        return;
    }
    doAccept();
    doTick();
}

Server::Server(asio::io_context &ioContext,
               const std::string &address,
               const std::string &port,
               const Settings &settings,
               size_t maxContentSize)
    : ioContext_(ioContext),
      acceptor_(ioContext),
      connectionManager_(settings),
      requestHandler_(maxContentSize),
      timer_(ioContext),
      maxContentSize_(maxContentSize),
      debugMsgCb_(defaultDebugMsgHandler) {
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    signals_ = std::make_shared<asio::signal_set>(ioContext);
    signals_->add(SIGINT);
    signals_->add(SIGTERM);
#if defined(SIGQUIT)
    signals_->add(SIGQUIT);
#endif  // defined(SIGQUIT)

    if (maxContentSize < 1024) {
        debugMsgCb_("maxContentSize must be equal or larger than 1024 bytes");
        return;
    }
    doAwaitStop();

    // Open the acceptor with the option to reuse the address (i.e.
    // SO_REUSEADDR).
    asio::ip::tcp::resolver resolver(ioContext);
    asio::ip::tcp::endpoint endpoint = *resolver.resolve(address, port).begin();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    doAccept();
    doTick();
}

Server::Server(asio::io_context &ioContext,
               const std::string &address,
               const std::string &port,
               asio::ssl::context &sslCtx,
               const Settings &settings,
               size_t maxContentSize)
    : ioContext_(ioContext),
      acceptor_(ioContext),
      sslCtx_(&sslCtx),
      connectionManager_(settings),
      requestHandler_(maxContentSize),
      timer_(ioContext),
      maxContentSize_(maxContentSize),
      debugMsgCb_(defaultDebugMsgHandler) {
    signals_ = std::make_shared<asio::signal_set>(ioContext);
    signals_->add(SIGINT);
    signals_->add(SIGTERM);
#if defined(SIGQUIT)
    signals_->add(SIGQUIT);
#endif

    if (maxContentSize < 1024) {
        debugMsgCb_("maxContentSize must be equal or larger than 1024 bytes");
        return;
    }
    doAwaitStop();

    asio::ip::tcp::resolver resolver(ioContext);
    asio::ip::tcp::endpoint endpoint = *resolver.resolve(address, port).begin();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    doAccept();
    doTick();
}

uint16_t Server::getBindedPort() const {
    return acceptor_.local_endpoint().port();
}

void Server::setFileIO(IFileIO *fileIO) {
    requestHandler_.setFileIO(fileIO);
}

void Server::addRequestHandler(const handlerCallback &cb) {
    requestHandler_.addRequestHandler(cb);
}

void Server::setExpectContinueHandler(const handlerCallback &cb) {
    requestHandler_.setExpectContinueHandler(cb);
}

void Server::setWsEndpoints(std::set<std::shared_ptr<WsEndpoint>> endpoints) {
    connectionManager_.setWsEndpoints(endpoints);
}

void Server::setDebugMsgHandler(const debugMsgCallback &cb) {
    connectionManager_.setDebugMsgHandler(cb);
    debugMsgCb_ = cb;
}

void Server::doAccept() {
#ifdef BEAUTY_HAS_SSL
    if (sslCtx_) {
        // SSL accept: pre-create the SslSocket so we can accept on its
        // underlying TCP socket, then let Connection::start() perform the
        // TLS handshake before the first read.
        auto sock = std::unique_ptr<SslSocket>(new SslSocket(ioContext_, *sslCtx_));
        auto *rawTcp = &sock->tcpSocket();
        acceptor_.async_accept(
            *rawTcp,
            [this, sock = std::move(sock)](std::error_code ec) mutable {
                if (!acceptor_.is_open()) {
                    return;
                }
                if (!ec) {
                    connectionManager_.start(std::make_shared<Connection>(std::move(sock),
                                                                         connectionManager_,
                                                                         requestHandler_,
                                                                         connectionId_++,
                                                                         maxContentSize_));
                } else {
                    debugMsgCb_("doAccept(ssl): " + ec.message() + ":" +
                                std::to_string(ec.value()));
                }
                doAccept();
            });
        return;
    }
#endif
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (!acceptor_.is_open()) {
            return;
        }
        if (!ec) {
            auto sock = std::unique_ptr<PlainSocket>(new PlainSocket(std::move(socket)));
            connectionManager_.start(std::make_shared<Connection>(std::move(sock),
                                                                  connectionManager_,
                                                                  requestHandler_,
                                                                  connectionId_++,
                                                                  maxContentSize_));
        } else {
            debugMsgCb_("doAccept: " + ec.message() + ":" + std::to_string(ec.value()));
        }
        doAccept();
    });
}

void Server::doAwaitStop() {
    signals_->async_wait([this](std::error_code /*ec*/, int /*signo*/) {
        timer_.cancel();
        acceptor_.close();
        connectionManager_.stopAll();
    });
}

void Server::doTick() {
    timer_.expires_after(std::chrono::seconds(1));
    timer_.async_wait([this](std::error_code ec) {
        if (!ec) {
            connectionManager_.tick();

            doTick();
        }
    });
}

}  // namespace beauty
