// Example: a minimal WebSocket data client built on the Beauty library.
//
// It connects to a ws:// URL, sends any messages given on the command line (or
// a default greeting), prints everything the server sends back, and supports
// automatic reconnection. It is well suited to the data endpoint exposed by the
// server demo (examples/pc/main.cpp) at ws://localhost:<port>/ws/data, where a
// single command (e.g. "burst" or "stats") triggers a stream of responses.
//
// For interactive, back-and-forth chat use the companion chat client
// (ws_chat_client_main.cpp / beauty_ws_chat_client_example) instead.
//
// Usage:
//   beauty_ws_client_example ws://127.0.0.1:8080/ws/data ["message" ...]

#include <atomic>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>

#include <asio.hpp>

#include <beauty/default_random.hpp>
#include <beauty/i_ws_client_handler.hpp>
#include <beauty/ws_client.hpp>
#include <beauty/ws_message.hpp>

using namespace beauty;

// Handler that prints connection events and echoes the configured messages
// once the connection is open.
class PrintingHandler : public IWsClientHandler {
   public:
    explicit PrintingHandler(std::vector<std::string> messages) : messages_(std::move(messages)) {}

    void onWsOpen() override {
        std::cout << "[open] connected\n";
        for (const auto &msg : messages_) {
            std::cout << "[send] " << msg << "\n";
            client_->sendText(msg);
        }
    }

    void onWsMessage(const WsMessage &message) override {
        std::string text(message.content_.begin(), message.content_.end());
        std::cout << "[recv] " << text << "\n";
    }

    void onWsClose() override {
        std::cout << "[close] connection closed\n";
    }

    void onWsError(const std::string &error) override {
        std::cout << "[error] " << error << "\n";
    }

    void setClient(std::shared_ptr<WsClient> client) {
        client_ = std::move(client);
    }

   private:
    std::vector<std::string> messages_;
    std::shared_ptr<WsClient> client_;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " ws://host:port/path [message ...]\n";
        return 1;
    }

    std::string url = argv[1];
    std::vector<std::string> messages;
    for (int i = 2; i < argc; ++i) {
        messages.emplace_back(argv[i]);
    }
    if (messages.empty()) {
        messages.emplace_back("Hello from the Beauty WebSocket client!");
    }

    try {
        asio::io_context ioc;

        DefaultRandom random;
        PrintingHandler handler(messages);

        WsClient::Config config;
        config.autoReconnect = true;
        config.reconnectInitialDelay = std::chrono::milliseconds(500);
        config.reconnectMaxDelay = std::chrono::seconds(10);

        auto client = WsClient::create(ioc, random, handler, config);
        handler.setClient(client);

        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](std::error_code /*ec*/, int /*signo*/) {
            std::cout << "\nShutting down client...\n";
            client->close();
            ioc.stop();
        });

        std::cout << "Connecting to " << url << " ...\n";
        client->connect(url);

        ioc.run();
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
