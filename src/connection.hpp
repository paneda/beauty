#pragma once
#include "environment.hpp"

#include <asio.hpp>
#include <chrono>
#include <vector>
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
    void start(bool useKeepAlive, std::chrono::seconds keepAliveTimeout, size_t keepAliveMax);

    // Stop all asynchronous operations associated with the connection.
    void stop();

    std::chrono::steady_clock::time_point getLastReceivedTime() const;
    size_t getNrOfRequests() const;
    bool useKeepAlive() const;

   private:
    // Perform an asynchronous read operation.
    void doRead();
    void doReadBody();

    // Perform an asynchronous write operation.
    void doWritePartAck();
    void doWriteHeaders();
    void doWriteContent();

    void handleKeepAlive();
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

    // Last received data timestamp.
    std::chrono::steady_clock::time_point lastReceivedTime_;

    // Number of seconds to keep connection open during inactivity.
    std::chrono::seconds keepAliveTimeout_;

    // Support keep-alive or not.
    bool useKeepAlive_ = false;

    // Max requests that can be made on the connection.
    size_t keepAliveMax_;

    // Request counter
    size_t nrOfRequest_ = 0;

    // The max buffer size when reading/writing socket.
    size_t maxContentSize_;
};

}  // namespace server
}  // namespace http
