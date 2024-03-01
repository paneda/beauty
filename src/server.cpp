#include "server.hpp"

#include <signal.h>

#include <utility>

namespace http {
namespace server {

Server::Server(asio::io_context &ioContext,
               const std::string &address,
               const std::string &port,
               const std::string &docRoot,
               IFileHandler &fileHandler)
    : signals_(ioContext),
      acceptor_(ioContext),
      connectionManager_(),
      requestHandler_(docRoot, fileHandler) {
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif  // defined(SIGQUIT)

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
}

void Server::doAccept() {
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        // Check whether the server was stopped by a signal before this
        // completion handler had a chance to run.
        if (!acceptor_.is_open()) {
            return;
        }

        if (!ec) {
            connectionManager_.start(std::make_shared<Connection>(
                std::move(socket), connectionManager_, requestHandler_));
        }

        doAccept();
    });
}

void Server::doAwaitStop() {
    signals_.async_wait([this](std::error_code /*ec*/, int /*signo*/) {
        // The server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the
        // io_context::run() call will exit.
        acceptor_.close();
        connectionManager_.stopAll();
    });
}

}  // namespace server
}  // namespace http
