#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "beauty/reply.hpp"
#include "beauty/request.hpp"

namespace beauty {

using handlerCallback = std::function<void(const Request &req, Reply &rep)>;

using debugMsgCallback = std::function<void(const std::string &msg)>;

struct Settings {
    Settings(std::chrono::seconds keepAliveTimeout = std::chrono::seconds(5),
             size_t keepAliveMax = 100,
             size_t connectionLimit = 0,
             std::chrono::seconds wsReceiveTimeout = std::chrono::seconds(300),
             std::chrono::seconds wsPingInterval = std::chrono::seconds(100),
             std::chrono::seconds wsPongTimeout = std::chrono::seconds(5))
        : keepAliveTimeout_(keepAliveTimeout),
          keepAliveMax_(keepAliveMax),
          connectionLimit_(connectionLimit),
          wsReceiveTimeout_(wsReceiveTimeout),
          wsPingInterval_(wsPingInterval),
          wsPongTimeout_(wsPongTimeout) {}

    // Keep-Alive timeout for inactive connections. Sent in Keep-Alive response header.
    // 0s = Keep-Alive disabled.
    std::chrono::seconds keepAliveTimeout_;

    // Max number of request that can be processed on the connection before it is closed.
    // Sent in Keep-Alive response header.
    size_t keepAliveMax_;

    // Internal limitation of the number of persistent http connections
    // that are allowed. If this limit is exceeded, Connection=close will be
    // sent in the response for new connections.
    // 0 = no limit.
    size_t connectionLimit_;

    // Maximum duration to keep a WebSocket connection open without receiving
    // any data (excluding pong responses) from the client. 0s = no timeout.
    std::chrono::seconds wsReceiveTimeout_;

    // Interval for sending ping frames to verify client responsiveness.
    // Should be significantly less than timeout_ (typically timeout_/3).
    // 0s = disable automatic ping (client activity only).
    std::chrono::seconds wsPingInterval_;

    // How long to wait for pong response after sending ping.
    // If no pong received within this time, connection is closed.
    std::chrono::seconds wsPongTimeout_;
};

}  // namespace beauty
