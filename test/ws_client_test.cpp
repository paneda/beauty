#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <deque>
#include <string>
#include <vector>

#include "beauty/default_random.hpp"
#include "beauty/i_ws_client_handler.hpp"
#include "beauty/server.hpp"
#include "beauty/ws_client.hpp"
#include "beauty/ws_endpoint.hpp"
#include "beauty/ws_message.hpp"

using namespace std::literals::chrono_literals;
using namespace beauty;

namespace {

// A WebSocket endpoint that echoes back every message it receives.
class EchoEndpoint : public WsEndpoint {
   public:
    EchoEndpoint() : WsEndpoint("/echo") {}

    void onWsOpen(const std::string &) override {}

    void onWsMessage(const std::string &connectionId, const WsMessage &wsMessage) override {
        std::string message(wsMessage.content_.begin(), wsMessage.content_.end());
        sendText(connectionId, "echo:" + message);
    }

    void onWsClose(const std::string &) override {}
    void onWsError(const std::string &, const std::string &) override {}
};

// Test handler that drives a scripted exchange: it sends each queued message
// and, once all expected echoes have been received, closes the connection.
class TestHandler : public IWsClientHandler {
   public:
    TestHandler(asio::io_context &ioc, std::deque<std::string> toSend)
        : ioc_(ioc), toSend_(std::move(toSend)) {}

    void onWsOpen() override {
        opened_ = true;
        sendNext();
    }

    void onWsMessage(const WsMessage &message) override {
        received_.emplace_back(message.content_.begin(), message.content_.end());
        if (!toSend_.empty()) {
            sendNext();
        } else {
            client_->close();
        }
    }

    void onWsClose() override {
        closed_ = true;
        ioc_.stop();
    }

    void onWsError(const std::string &error) override {
        error_ = error;
        ioc_.stop();
    }

    void setClient(std::shared_ptr<WsClient> client) {
        client_ = std::move(client);
    }

    void sendNext() {
        std::string msg = toSend_.front();
        toSend_.pop_front();
        client_->sendText(msg);
    }

    bool opened_ = false;
    bool closed_ = false;
    std::string error_;
    std::vector<std::string> received_;

   private:
    asio::io_context &ioc_;
    std::deque<std::string> toSend_;
    std::shared_ptr<WsClient> client_;
};

}  // namespace

TEST_CASE("websocket client round-trip", "[ws_client]") {
    asio::io_context ioc;
    Settings settings(0s, 0, 0);
    Server server(ioc, "127.0.0.1", "0", settings);
    uint16_t port = server.getBindedPort();
    REQUIRE(port != 0);

    auto echo = std::make_shared<EchoEndpoint>();
    server.setWsEndpoints({echo});

    DefaultRandom random;

    // Watchdog so a failing test never hangs the whole suite.
    asio::steady_timer watchdog(ioc);
    watchdog.expires_after(3s);
    watchdog.async_wait([&ioc](const std::error_code &ec) {
        if (!ec) {
            ioc.stop();
        }
    });

    SECTION("should complete handshake and echo a text message") {
        TestHandler handler(ioc, {"hello"});
        WsClient::Config config;
        config.autoReconnect = false;
        auto client = WsClient::create(ioc, random, handler, config);
        handler.setClient(client);

        client->connect("ws://127.0.0.1:" + std::to_string(port) + "/echo");
        ioc.run();

        REQUIRE(handler.error_.empty());
        REQUIRE(handler.opened_);
        REQUIRE(handler.closed_);
        REQUIRE(handler.received_.size() == 1);
        REQUIRE(handler.received_[0] == "echo:hello");
    }

    SECTION("should echo multiple messages in order") {
        TestHandler handler(ioc, {"one", "two", "three"});
        WsClient::Config config;
        config.autoReconnect = false;
        auto client = WsClient::create(ioc, random, handler, config);
        handler.setClient(client);

        client->connect("ws://127.0.0.1:" + std::to_string(port) + "/echo");
        ioc.run();

        REQUIRE(handler.error_.empty());
        REQUIRE(handler.received_.size() == 3);
        REQUIRE(handler.received_[0] == "echo:one");
        REQUIRE(handler.received_[1] == "echo:two");
        REQUIRE(handler.received_[2] == "echo:three");
    }

    SECTION("should report error for unknown endpoint path") {
        TestHandler handler(ioc, {"hello"});
        WsClient::Config config;
        config.autoReconnect = false;
        auto client = WsClient::create(ioc, random, handler, config);
        handler.setClient(client);

        client->connect("ws://127.0.0.1:" + std::to_string(port) + "/does-not-exist");
        ioc.run();

        REQUIRE_FALSE(handler.error_.empty());
        REQUIRE_FALSE(handler.opened_);
    }
}
