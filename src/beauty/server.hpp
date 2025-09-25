#pragma once
// included first
#include "beauty/environment.hpp"

#include <asio.hpp>
#include <memory>
#include <string>

#include "beauty/beauty_common.hpp"
#include "beauty/connection.hpp"
#include "beauty/connection_manager.hpp"
#include "beauty/i_file_io.hpp"
#include "beauty/request_handler.hpp"

namespace beauty {

class Server {
   public:
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    // Simple constructor, use for ESP32.
    explicit Server(asio::io_context &ioContext,
                    uint16_t port,
                    HttpPersistence options,
                    size_t maxContentSize = 1024);

    // Advanced constructor use for OS:s supporting signal_set.
    explicit Server(asio::io_context &ioContext,
                    const std::string &address,
                    const std::string &port,
                    HttpPersistence options,
                    size_t maxContentSize = 1024);
    ~Server() = default;

    uint16_t getBindedPort() const;

    // Handlers to be optionally implemented.
    void setFileIO(IFileIO *fileIO);
    void addRequestHandler(const handlerCallback &cb);
    void setExpectContinueHandler(const handlerCallback &cb);
    void setDebugMsgHandler(const debugMsgCallback &cb);

   private:
    void doAccept();
    void doAwaitStop();
    void doTick();

    std::shared_ptr<asio::signal_set> signals_;
    asio::ip::tcp::acceptor acceptor_;
    ConnectionManager connectionManager_;
    RequestHandler requestHandler_;

    // Unique Id for each connection.
    unsigned connectionId_ = 0;

    // Timer to handle connection status.
    asio::steady_timer timer_;

    // The max buffer size when reading/writing socket.
    const size_t maxContentSize_;

    // Callback to handle post file access, e.g. a custom not found handler.
    handlerCallback fileNotFoundCb_;

    // Callback to handle debug messages.
    debugMsgCallback debugMsgCb_;
};

}  // namespace beauty
