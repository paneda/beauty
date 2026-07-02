#pragma once
// included first
#include "beauty/environment.hpp"

#include <asio.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "beauty/i_ws_client_handler.hpp"
#include "beauty/response.hpp"
#include "beauty/response_parser.hpp"
#include "beauty/url_parser.hpp"
#include "beauty/ws_encoder.hpp"
#include "beauty/ws_message.hpp"
#include "beauty/ws_parser.hpp"
#include "beauty/ws_types.hpp"

namespace beauty {

class IRandom;

// A WebSocket client that runs on a (shared) asio::io_context, following the
// same philosophy as the Beauty server: fixed maximum size receive/send buffers
// and fully asynchronous, callback based operation.
//
// The client is intended to be owned through a std::shared_ptr so that its
// lifetime is guaranteed for the duration of outstanding asynchronous
// operations. Use WsClient::create() to construct one.
class WsClient : public std::enable_shared_from_this<WsClient> {
   public:
    struct Config {
        // Maximum size of the receive and send buffers, in bytes. Also bounds
        // the largest message that can be received/sent in a single frame.
        size_t maxMessageSize = 1024;

        // Automatically reconnect after the connection is lost or fails.
        bool autoReconnect = true;

        // Backoff parameters for auto-reconnect.
        std::chrono::milliseconds reconnectInitialDelay = std::chrono::milliseconds(500);
        std::chrono::milliseconds reconnectMaxDelay = std::chrono::milliseconds(30000);

        // Interval for automatically sending ping frames to keep the connection
        // alive and detect a dead peer. 0 = disabled.
        std::chrono::seconds pingInterval = std::chrono::seconds(0);
    };

    WsClient(const WsClient &) = delete;
    WsClient &operator=(const WsClient &) = delete;

    static std::shared_ptr<WsClient> create(asio::io_context &ioContext,
                                            IRandom &random,
                                            IWsClientHandler &handler,
                                            const Config &config);

    static std::shared_ptr<WsClient> create(asio::io_context &ioContext,
                                            IRandom &random,
                                            IWsClientHandler &handler);

    ~WsClient() = default;

    // Start connecting to the given ws:// URL. May be called again to point the
    // client at a different URL (an existing connection is closed first).
    void connect(const std::string &url);

    // Send a text frame. Returns SUCCESS if the write was started, otherwise
    // WRITE_IN_PROGRESS or CONNECTION_CLOSED.
    WriteResult sendText(const std::string &text);

    // Send a binary frame.
    WriteResult sendBinary(const std::vector<char> &data);

    // Send a ping frame (no-op if not open or a write is already in progress).
    void sendPing();

    // Initiate a graceful close. Disables auto-reconnect for this close.
    void close(uint16_t statusCode = 1000, const std::string &reason = "");

    bool isOpen() const {
        return isOpen_;
    }

   private:
    WsClient(asio::io_context &ioContext,
             IRandom &random,
             IWsClientHandler &handler,
             const Config &config);

    void doResolve();
    void doConnect(const asio::ip::tcp::resolver::results_type &endpoints);
    void doWriteHandshake();
    void doReadHandshake();
    void processWsBuffer();
    void doReadWs();
    void doWriteWsFrame(bool continueReading, WriteCompleteCallback callback);
    void startPingTimer();

    void finishConnection(bool notifyClose);
    void handleDisconnect(const std::string &error);
    void scheduleReconnect();

    void resetForNewConnection();

    asio::io_context &ioContext_;
    asio::ip::tcp::resolver resolver_;
    asio::ip::tcp::socket socket_;
    asio::steady_timer reconnectTimer_;
    asio::steady_timer pingTimer_;

    IRandom &random_;
    IWsClientHandler &handler_;
    Config config_;

    // Buffers (fixed maximum size, mirroring the server).
    std::vector<char> recvBuffer_;  // Socket reads, response parsing input, WS frames
    std::vector<char> sendBuffer_;  // Handshake request and outgoing WS frames
    std::vector<char> bodyBuffer_;  // Handshake response body

    UrlParser url_;
    std::string host_;
    std::string port_;
    std::string path_;
    std::string secKey_;

    Response response_;
    ResponseParser responseParser_;
    WsEncoder wsEncoder_;
    WsMessage wsMessage_;
    WsParser wsParser_;

    bool isOpen_ = false;
    bool handshakeDone_ = false;
    bool writeInProgress_ = false;
    bool closing_ = false;
    bool reconnectEnabled_ = true;
    bool finishing_ = false;
    bool closePending_ = false;

    // Offset into recvBuffer_ where the next socket read should place data.
    // Non-zero when WsParser returned indeterminate (partial frame) and
    // partially decoded payload bytes occupy [0, wsParseOffset_).
    size_t wsParseOffset_ = 0;

    // Deferred close parameters (used when close() is called while a write
    // is in progress).
    uint16_t pendingCloseCode_ = 1000;
    std::string pendingCloseReason_;

    std::chrono::milliseconds currentReconnectDelay_;
};

inline std::shared_ptr<WsClient> WsClient::create(asio::io_context &ioContext,
                                                  IRandom &random,
                                                  IWsClientHandler &handler,
                                                  const Config &config) {
    return std::shared_ptr<WsClient>(new WsClient(ioContext, random, handler, config));
}

inline std::shared_ptr<WsClient> WsClient::create(asio::io_context &ioContext,
                                                  IRandom &random,
                                                  IWsClientHandler &handler) {
    return std::shared_ptr<WsClient>(new WsClient(ioContext, random, handler, Config()));
}

}  // namespace beauty
