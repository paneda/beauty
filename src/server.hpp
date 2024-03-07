#pragma once
// clang-format off
#include "environment.hpp"
// clang-format on

#include <asio.hpp>
#include <memory>
#include <string>

#include "beauty_common.hpp"
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

    // simple constructor, use for ESP32
    explicit Server(asio::io_context &ioContext,
                    uint16_t port,
                    const std::string &fileRoot,
                    IFileHandler *fileHandler,
                    const std::string &routeRoot,
                    IRouteHandler *routeHandler);

    // advanced constructor use for OS:s supporting signal_set
    explicit Server(asio::io_context &ioContext,
                    const std::string &address,
                    const std::string &port,
                    const std::string &fileRoot,
                    IFileHandler *fileHandler,
                    const std::string &routeRoot,
                    IRouteHandler *routeHandler);

    uint16_t getBindedPort() const;

    // handlers to be implemented by each specific project
    void addHeaderHandler(addHeaderCallback cb);

   private:
    void doAccept();
    void doAwaitStop();

    std::shared_ptr<asio::signal_set> signals_;
    asio::ip::tcp::acceptor acceptor_;
    ConnectionManager connectionManager_;
    RequestHandler requestHandler_;
    // each connection gets a unique internal id
    unsigned connectionId_ = 0;
};

}  // namespace server
}  // namespace http
