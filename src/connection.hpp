#pragma once
#include "environment.hpp"

#include <array>
#include <asio.hpp>
#include <memory>

#include "reply.hpp"
#include "request.hpp"
#include "request_decoder.hpp"
#include "request_handler.hpp"
#include "request_parser.hpp"

namespace http {
namespace server {

class ConnectionManager;

// Represents a single connection from a client.
class Connection : public std::enable_shared_from_this<Connection> {
   public:
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    // Construct a connection with the given socket.
    explicit Connection(asio::ip::tcp::socket socket,
                        ConnectionManager &manager,
                        RequestHandler &handler,
                        unsigned connectionId,
                        size_t maxContentSize);

    // Start the first asynchronous operation for the connection.
    void start();

    // Stop all asynchronous operations associated with the connection.
    void stop();

   private:
    // Perform an asynchronous read operation.
    void doRead();
    void doReadBody();

    // Perform an asynchronous write operation.
    void doWritePartAck();
    void doWriteHeaders();
    void doWriteContent();

    void handleWriteCompleted();

    void shutdown();

    // Socket for the connection.
    asio::ip::tcp::socket socket_;

    // The manager for this connection.
    ConnectionManager &connectionManager_;

    // The handler used to process the incoming request.
    RequestHandler &requestHandler_;

    // Buffer for incoming data.
    std::vector<char> buffer_;

    // The incoming request.
    Request request_;

    // The parser for the incoming request.
    RequestParser requestParser_;

    // The decoder for the incoming request.
    RequestDecoder requestDecoder_;

    // The reply to be sent back to the client.
    Reply reply_;

    // The unique id for the connection.
    unsigned connectionId_;

    // The max buffer size when reading from socket.
    size_t maxContentSize_;
};

typedef std::shared_ptr<Connection> connection_ptr;

}  // namespace server
}  // namespace http
