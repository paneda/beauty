#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

using handlerCallback = std::function<void(const Request &req, Reply &rep)>;

using debugMsgCallback = std::function<void(const std::string &msg)>;
static void defaultDebugMsgHandler(const std::string &msg) {}

struct HttpPersistence {
    HttpPersistence(std::chrono::seconds keepAliveTimeout,
                    size_t keepAliveMax,
                    size_t connectionLimit)
        : keepAliveTimeout_(keepAliveTimeout),
          keepAliveMax_(keepAliveMax),
          connectionLimit_(connectionLimit) {}

    // Keep-Alive timeout for inactive connections. Sent in Keep-Alive response header.
    // 0s = Keep-Alive disabled.
    std::chrono::seconds keepAliveTimeout_;

    // Max number of request that can be sent before closing the connection.
    // Sent in Keep-Alive response header.
    size_t keepAliveMax_;

    // Internal limitation of the number of persistent http connections
    // that are allowed. If this limit is exceeded, Connection=close will be
    // sent to new clients.
    // 0 = no limit.
    size_t connectionLimit_;
};

}  // namespace server
}  // namespace http
