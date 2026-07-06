#pragma once

#include <string>

namespace beauty {

struct Response;

// Callback interface implemented by the application to receive HTTP client
// events. All callbacks are invoked on the thread that runs the asio
// io_context, mirroring the WebSocket client interface.
class IHttpClientHandler {
   public:
    virtual ~IHttpClientHandler() = default;

    // Called once a complete HTTP response has been received. The response and
    // its body buffer are only valid for the duration of this callback.
    virtual void onResponse(const Response &response) = 0;

    // Called when an error occurs during resolve, connect, sending the request
    // or receiving the response (including timeouts and oversized responses).
    virtual void onError(const std::string &error) = 0;
};

}  // namespace beauty
