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
             size_t keepAliveThresholdPercent = 75,
             std::chrono::seconds wsReceiveTimeout = std::chrono::seconds(300),
             std::chrono::seconds wsPingInterval = std::chrono::seconds(100),
             std::chrono::seconds wsPongTimeout = std::chrono::seconds(5))
        : keepAliveTimeout_(keepAliveTimeout),
          keepAliveMax_(keepAliveMax),
          connectionLimit_(connectionLimit),
          keepAliveThresholdPercent_(keepAliveThresholdPercent),
          wsReceiveTimeout_(wsReceiveTimeout),
          wsPingInterval_(wsPingInterval),
          wsPongTimeout_(wsPongTimeout) {}

    // Keep-Alive timeout for inactive connections. Sent in Keep-Alive response header.
    // 0s = Keep-Alive disabled.
    std::chrono::seconds keepAliveTimeout_;

    // Max number of request that can be processed on the connection before it is closed.
    // Sent in Keep-Alive response header.
    size_t keepAliveMax_;

    // Hard limit on the number of concurrent connections. Once this many
    // connections are open, further incoming connections are refused with
    // 503 Service Unavailable (and closed) before their buffers are allocated,
    // bounding memory use. Size it from free heap and the per-connection cost
    // (roughly 2 * maxContentSize). This is critical for embedded targets.
    // 0 = no limit.
    size_t connectionLimit_;

    // Soft threshold for graceful degradation, as a percentage of
    // connectionLimit_. Once the number of open connections reaches this
    // percentage of the hard cap, new connections are served without keep-alive
    // (Connection: close) so they release their slot immediately after one
    // request, instead of holding their buffers for keepAliveTimeout_. This
    // keeps the server responsive to new clients as it approaches the hard cap.
    // 0 = disabled (keep-alive offered right up to the cap). Ignored when
    // connectionLimit_ is 0, since there is no cap to approach.
    size_t keepAliveThresholdPercent_;

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

// Decides whether a newly accepted connection should be offered HTTP keep-alive,
// given the number of open connections including this one. Keep-alive is offered
// unless it is disabled (keepAliveTimeout_ == 0), or the open-connection count
// has reached the soft threshold (keepAliveThresholdPercent_ of
// connectionLimit_), at which point connections are served with Connection:
// close so they free their slot promptly as the server approaches the hard cap.
// The soft threshold is ignored when there is no hard cap (connectionLimit_ == 0)
// or when it is disabled (keepAliveThresholdPercent_ == 0). The cross-multiplied
// comparison avoids integer truncation when connectionLimit_ is small.
inline bool keepAliveAllowed(const Settings& settings, size_t connectionCount) {
    if (settings.keepAliveTimeout_ == std::chrono::seconds(0)) {
        return false;
    }
    if (settings.connectionLimit_ == 0 || settings.keepAliveThresholdPercent_ == 0) {
        return true;
    }
    return connectionCount * 100 < settings.connectionLimit_ * settings.keepAliveThresholdPercent_;
}

}  // namespace beauty
