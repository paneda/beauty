#pragma once

#include <asio.hpp>
#include <string>

#include "connection.hpp"
#include "connection_manager.hpp"
#include "i_file_handler.hpp"
#include "request_handler.hpp"

namespace http {
namespace server {

class Server {
   public:
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    /// Construct the server to listen on the specified TCP address and port,
    /// and serve up files from the given directory.
    explicit Server(asio::io_context &ioContext,
                    const std::string &address,
                    const std::string &port,
                    const std::string &docRoot,
                    IFileHandler &fileHandler);

    uint16_t getBindedPort() const {
        return acceptor_.local_endpoint().port();
    }

   private:
    /// Perform an asynchronous accept operation.
    void doAccept();

    /// Wait for a request to stop the server.
    void doAwaitStop();

    /// The io_context used to perform asynchronous operations.
    // asio::io_context ioContext_;

    /// The signal_set is used to register for process termination
    /// notifications.
    asio::signal_set signals_;

    /// Acceptor used to listen for incoming connections.
    asio::ip::tcp::acceptor acceptor_;

    /// The connection manager which owns all live connections.
    ConnectionManager connectionManager_;

    /// The handler for all incoming requests.
    RequestHandler requestHandler_;
};

}  // namespace server
}  // namespace http
