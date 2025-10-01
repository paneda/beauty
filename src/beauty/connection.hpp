#pragma once
#include "beauty/environment.hpp"

#include <asio.hpp>
#include <chrono>
#include <vector>
#include <memory>

#include "beauty/reply.hpp"
#include "beauty/request.hpp"
#include "beauty/request_decoder.hpp"
#include "beauty/request_handler.hpp"
#include "beauty/request_parser.hpp"
#include "beauty/ws_message.hpp"
#include "beauty/ws_parser.hpp"
#include "beauty/i_ws_receiver.hpp"
#include "beauty/ws_types.hpp"
#include "beauty/ws_encoder.hpp"

namespace beauty {

class ConnectionManager;
class WsEndpoint;

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
    ~Connection() = default;

    // Start the first asynchronous operation for the connection.
    void start(bool useKeepAlive, std::chrono::seconds keepAliveTimeout, size_t keepAliveMax);

    // Stop all asynchronous operations associated with the connection.
    void stop();

    std::chrono::steady_clock::time_point getLastActivityTime() const;
    std::chrono::steady_clock::time_point getLastReceivedTime() const;
    std::chrono::steady_clock::time_point getLastPingTime() const;
    std::chrono::steady_clock::time_point getLastPongTime() const;

    size_t getNrOfRequests() const;
    bool useKeepAlive() const;
    bool isWebSocket() const;
    unsigned getConnectionId() const;
    WsEndpoint *getWsEndpoint() const;
    void sendWsPing();

    // WebSocket write state query
    bool isWriteInProgress() const;

    WriteResult sendWsText(const std::string &message, WriteCompleteCallback callback);
    WriteResult sendWsBinary(const std::vector<char> &data, WriteCompleteCallback callback);
    WriteResult sendWsClose(uint16_t statusCode = 1000,
                            const std::string &reason = "",
                            WriteCompleteCallback callback = nullptr);

   private:
    // Perform an asynchronous read operation.
    void doRead();
    void doReadBody();
    void doReadBodyAfter100Continue();

    // Perform an asynchronous write operation.
    void doWriteHeaders();
    void doWriteReplyContent();
    void doWrite100Continue();

    void handleConnection();
    void handleWriteCompleted();

    void handleUpgradeToWebSocket();
    void doAckWsUpgrade();
    void doWriteWsFrame(bool continueReading = false, WriteCompleteCallback callback = nullptr);

    void shutdown();

    // Socket for the connection.
    asio::ip::tcp::socket socket_;

    // The manager for this connection.
    ConnectionManager &connectionManager_;

    // The handler used to process the incoming request.
    RequestHandler &requestHandler_;

    // The unique id for the connection.
    unsigned connectionId_;

    // The max buffer size when reading/writing socket.
    size_t maxContentSize_;

    // Buffer for incoming data. HTTP requests + WebSocket incoming frames
    std::vector<char> recvBuffer_;

    // Buffer for outgoing data. HTTP responses + WebSocket outgoing frames
    std::vector<char> sendBuffer_;

    // The incoming request.
    Request request_;

    // The parser for the incoming request.
    RequestParser requestParser_;

    // The decoder for the incoming request.
    RequestDecoder requestDecoder_;

    // The reply to be sent back to the client.
    Reply reply_;

    // The encoder for the outgoing WebSocket frames.
    WsEncoder wsEncoder_;

    // Last connection activity timestamp.
    std::chrono::steady_clock::time_point lastActivityTime_;

    // Last received data timestamp.
    std::chrono::steady_clock::time_point lastReceivedTime_;

    // Last sent ping timestamp.
    std::chrono::steady_clock::time_point lastPingTime_;

    // Last received pong timestamp.
    std::chrono::steady_clock::time_point lastPongTime_;

    // The WebSocket endpoint for this connection (set during upgrade).
    WsEndpoint *wsEndpoint_ = nullptr;

    // The received message over WebSocket.
    WsMessage wsMessage_;

    // The parser for the incoming web socket data.
    WsParser wsParser_;

    // Number of seconds to keep connection open during inactivity.
    std::chrono::seconds keepAliveTimeout_;

    // Support keep-alive or not.
    bool useKeepAlive_ = false;

    // Max requests that can be made on the connection.
    size_t keepAliveMax_;

    // Request counter
    size_t nrOfRequest_ = 0;

    bool closeConnection_ = false;

    bool firstBodyReadAfter100Continue_ = true;

    bool isWebSocket_ = false;

    // WebSocket write state tracking
    bool writeInProgress_ = false;
    WriteCompleteCallback writeCallback_;
};

}  // namespace beauty
