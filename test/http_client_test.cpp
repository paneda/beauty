#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "beauty/http_client.hpp"
#include "beauty/i_http_client_handler.hpp"
#include "beauty/reply.hpp"
#include "beauty/request.hpp"
#include "beauty/response.hpp"
#include "beauty/server.hpp"

using namespace std::literals::chrono_literals;
using namespace beauty;

namespace {

// Records the outcome of an HTTP request and stops the io_context so the test
// body can inspect the result. A per-response callback allows a test to issue a
// follow-up request (e.g. to exercise keep-alive reuse).
class TestHttpHandler : public IHttpClientHandler {
   public:
    explicit TestHttpHandler(asio::io_context &ioc) : ioc_(ioc) {}

    void onResponse(const Response &response) override {
        ++responseCount_;
        lastStatus_ = response.statusCode_;
        lastBody_.assign(response.body_.begin(), response.body_.end());
        lastKeepAlive_ = response.keepAlive_;
        if (onResponseCb_) {
            onResponseCb_(response);
        } else {
            ioc_.stop();
        }
    }

    void onError(const std::string &error) override {
        error_ = error;
        ioc_.stop();
    }

    int responseCount_ = 0;
    int lastStatus_ = 0;
    std::string lastBody_;
    bool lastKeepAlive_ = false;
    std::string error_;
    std::function<void(const Response &)> onResponseCb_;

   private:
    asio::io_context &ioc_;
};

// Registers a request handler that serves a few fixed routes for the tests.
void installRoutes(Server &server) {
    server.addRequestHandler([](const Request &req, Reply &rep) {
        if (req.requestPath_ == "/hello") {
            std::string body = "hello world";
            rep.content_.assign(body.begin(), body.end());
            rep.send(Reply::ok, "text/plain");
        } else if (req.requestPath_ == "/echo" && req.method_ == "POST") {
            rep.content_.assign(req.body_.begin(), req.body_.end());
            rep.send(Reply::ok, "application/octet-stream");
        } else {
            rep.send(Reply::not_found);
        }
    });
}

}  // namespace

TEST_CASE("http client request/response", "[http_client]") {
    asio::io_context ioc;
    Settings settings(0s, 0, 0);  // keep-alive disabled: server closes after each response
    Server server(ioc, "127.0.0.1", "0", settings);
    uint16_t port = server.getBindedPort();
    REQUIRE(port != 0);
    installRoutes(server);

    const std::string base = "http://127.0.0.1:" + std::to_string(port);

    asio::steady_timer watchdog(ioc);
    watchdog.expires_after(3s);
    watchdog.async_wait([&ioc](const std::error_code &ec) {
        if (!ec) {
            ioc.stop();
        }
    });

    SECTION("should GET a body with status 200") {
        TestHttpHandler handler(ioc);
        auto client = HttpClient::create(ioc, handler);
        REQUIRE(client->get(base + "/hello"));
        ioc.run();

        REQUIRE(handler.error_.empty());
        REQUIRE(handler.responseCount_ == 1);
        REQUIRE(handler.lastStatus_ == 200);
        REQUIRE(handler.lastBody_ == "hello world");
    }

    SECTION("should GET a URL without http:// scheme") {
        TestHttpHandler handler(ioc);
        auto client = HttpClient::create(ioc, handler);
        std::string noScheme = "127.0.0.1:" + std::to_string(port) + "/hello";
        REQUIRE(client->get(noScheme));
        ioc.run();

        REQUIRE(handler.error_.empty());
        REQUIRE(handler.responseCount_ == 1);
        REQUIRE(handler.lastStatus_ == 200);
        REQUIRE(handler.lastBody_ == "hello world");
    }

    SECTION("should report 404 for an unknown path") {
        TestHttpHandler handler(ioc);
        auto client = HttpClient::create(ioc, handler);
        REQUIRE(client->get(base + "/nope"));
        ioc.run();

        REQUIRE(handler.error_.empty());
        REQUIRE(handler.lastStatus_ == 404);
    }

    SECTION("should POST a body and receive it echoed back") {
        TestHttpHandler handler(ioc);
        auto client = HttpClient::create(ioc, handler);
        REQUIRE(client->post(base + "/echo", "text/plain", "payload-123"));
        ioc.run();

        REQUIRE(handler.error_.empty());
        REQUIRE(handler.lastStatus_ == 200);
        REQUIRE(handler.lastBody_ == "payload-123");
    }
}

TEST_CASE("http client keep-alive reuse", "[http_client]") {
    asio::io_context ioc;
    Settings settings(5s, 100, 0);  // keep-alive enabled
    Server server(ioc, "127.0.0.1", "0", settings);
    uint16_t port = server.getBindedPort();
    REQUIRE(port != 0);
    installRoutes(server);

    const std::string base = "http://127.0.0.1:" + std::to_string(port);

    asio::steady_timer watchdog(ioc);
    watchdog.expires_after(3s);
    watchdog.async_wait([&ioc](const std::error_code &ec) {
        if (!ec) {
            ioc.stop();
        }
    });

    SECTION("should perform two sequential requests on one client") {
        TestHttpHandler handler(ioc);
        auto client = HttpClient::create(ioc, handler);

        handler.onResponseCb_ = [&](const Response &) {
            if (handler.responseCount_ == 1) {
                // Issue the second request from within the first response.
                client->get(base + "/hello");
            } else {
                ioc.stop();
            }
        };

        REQUIRE(client->get(base + "/hello"));
        ioc.run();

        REQUIRE(handler.error_.empty());
        REQUIRE(handler.responseCount_ == 2);
        REQUIRE(handler.lastStatus_ == 200);
        REQUIRE(handler.lastBody_ == "hello world");
    }
}

TEST_CASE("http client error handling", "[http_client]") {
    asio::io_context ioc;

    SECTION("should report an error for an invalid URL") {
        TestHttpHandler handler(ioc);
        auto client = HttpClient::create(ioc, handler);
        REQUIRE(client->get("http://"));  // no hostname
        ioc.run();
        REQUIRE_FALSE(handler.error_.empty());
    }

    SECTION("should report an error when the connection is refused") {
        // Port 1 is almost never listening; connect should fail quickly.
        TestHttpHandler handler(ioc);
        HttpClient::Config config;
        config.requestTimeout = 2s;
        auto client = HttpClient::create(ioc, handler, config);
        REQUIRE(client->get("http://127.0.0.1:1/"));
        ioc.run();
        REQUIRE_FALSE(handler.error_.empty());
    }
}
