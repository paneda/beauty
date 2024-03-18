#pragma once
// clang-format off
#include "environment.hpp"
// clang-format on

#include <array>
#include <asio.hpp>
#include <chrono>
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
                        unsigned connectionId);

    // Start the first asynchronous operation for the connection.
    void start(std::chrono::seconds keepAliveTimeout, size_t keepAliveMax);

    // Stop all asynchronous operations associated with the connection.
    void stop();

    std::chrono::steady_clock::time_point getLastRequestTime() const;
    size_t getNrOfRequests() const;

   private:
    // Perform an asynchronous read operation.
    void doRead();

    // Perform an asynchronous write operation.
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
    std::array<char, 8192> buffer_;

    // The incoming request.
    Request request_;

    // The parser for the incoming request.
    RequestParser requestParser_;

    // The decoder for the incoming request.
    RequestDecoder requestDecoder_;

    // The reply to be sent back to the client.
    Reply reply_;

    unsigned connectionId_;

    std::chrono::steady_clock::time_point timestamp_;

    std::chrono::seconds keepAliveTimeout_;
    size_t keepAliveMax_;
    size_t nrOfRequest_ = 0;
};

typedef std::shared_ptr<Connection> connection_ptr;

}  // namespace server
}  // namespace http
