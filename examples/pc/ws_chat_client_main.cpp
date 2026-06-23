// Example: an interactive WebSocket chat client built on the Beauty library.
//
// Unlike the data client (ws_client_main.cpp), which sends messages given on
// the command line and then waits, this client stays interactive: it reads
// lines you type on stdin and sends each one as a text frame, while printing
// everything the server sends back. It is intended for the chat endpoint
// exposed by the server demo (examples/pc/main.cpp) at ws://<host>/ws/chat.
//
// Type a line and press Enter to send it. Type "/quit" (or press Ctrl-C) to
// exit.
//
// Usage:
//   beauty_ws_chat_client_example ws://127.0.0.1:8080/ws/chat

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

#include <asio.hpp>

#include <beauty/default_random.hpp>
#include <beauty/i_ws_client_handler.hpp>
#include <beauty/ws_client.hpp>
#include <beauty/ws_message.hpp>

#if defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

using namespace beauty;

// Handler that prints connection lifecycle events and incoming messages.
class ChatHandler : public IWsClientHandler {
   public:
    void onWsOpen() override {
        std::cout << "[connected] type a message and press Enter, or /quit to exit\n";
        std::cout << "> " << std::flush;
    }

    void onWsMessage(const WsMessage &message) override {
        std::string text(message.content_.begin(), message.content_.end());
        // Print the incoming message on its own line, then restore the prompt.
        std::cout << "\r< " << text << "\n> " << std::flush;
    }

    void onWsClose() override {
        std::cout << "\n[disconnected]\n";
    }

    void onWsError(const std::string &error) override {
        std::cout << "\n[error] " << error << "\n";
    }
};

// Reads lines from stdin on the same io_context and forwards them to the client.
class ConsoleReader {
   public:
    ConsoleReader(asio::io_context &ioc, std::shared_ptr<WsClient> client)
        : ioc_(ioc), input_(ioc, ::dup(STDIN_FILENO)), client_(std::move(client)) {}

    void start() {
        readLine();
    }

   private:
    void readLine() {
        asio::async_read_until(
            input_, buffer_, '\n', [this](const std::error_code &ec, std::size_t) {
                if (ec) {
                    // EOF (Ctrl-D) or read error: close and stop.
                    client_->close();
                    ioc_.stop();
                    return;
                }

                std::istream stream(&buffer_);
                std::string line;
                std::getline(stream, line);

                if (line == "/quit") {
                    client_->close();
                    ioc_.stop();
                    return;
                }

                if (!line.empty()) {
                    if (client_->sendText(line) == WriteResult::CONNECTION_CLOSED) {
                        std::cout << "[not connected] message not sent\n";
                    }
                }
                std::cout << "> " << std::flush;
                readLine();
            });
    }

    asio::io_context &ioc_;
    asio::posix::stream_descriptor input_;
    asio::streambuf buffer_;
    std::shared_ptr<WsClient> client_;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " ws://host:port/path\n";
        return 1;
    }

    std::string url = argv[1];

    try {
        asio::io_context ioc;

        DefaultRandom random;
        ChatHandler handler;

        WsClient::Config config;
        config.autoReconnect = true;
        config.reconnectInitialDelay = std::chrono::milliseconds(500);
        config.reconnectMaxDelay = std::chrono::seconds(10);

        auto client = WsClient::create(ioc, random, handler, config);

        ConsoleReader reader(ioc, client);

        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](std::error_code /*ec*/, int /*signo*/) {
            std::cout << "\nShutting down chat client...\n";
            client->close();
            ioc.stop();
        });

        std::cout << "Connecting to " << url << " ...\n";
        client->connect(url);
        reader.start();

        ioc.run();
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

#else  // !ASIO_HAS_POSIX_STREAM_DESCRIPTOR

int main() {
    std::cerr << "This interactive chat client requires a POSIX platform "
                 "(asio::posix::stream_descriptor).\n";
    return 1;
}

#endif
