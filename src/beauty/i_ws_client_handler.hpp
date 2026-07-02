#pragma once

#include <cstdint>
#include <string>

namespace beauty {

struct WsMessage;

// Callback interface implemented by the application to receive WebSocket client
// events. All callbacks are invoked on the thread that runs the asio
// io_context, mirroring the server side WsEndpoint interface.
class IWsClientHandler {
   public:
    virtual ~IWsClientHandler() = default;

    // Called once the WebSocket handshake has completed successfully and the
    // connection is ready to send and receive frames.
    virtual void onWsOpen() = 0;

    // Called for every received data frame (text or binary). The message buffer
    // is only valid for the duration of the callback.
    virtual void onWsMessage(const WsMessage &message) = 0;

    // Called when the connection has been closed (either by the peer or after a
    // local close). If auto-reconnect is enabled the client will attempt to
    // reconnect after this callback returns.
    virtual void onWsClose() = 0;

    // Called when an error occurs during connect, handshake or while the
    // connection is open.
    virtual void onWsError(const std::string &error) = 0;
};

}  // namespace beauty
