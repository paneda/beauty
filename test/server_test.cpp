#include "server.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <csignal>
#include <future>
#include <thread>

#include "utils/test_client.hpp"

using namespace std::literals::chrono_literals;
namespace {
std::future<TestClient::TestResult> createFutureResult(TestClient& client) {
    auto fut = std::async(std::launch::async, std::bind(&TestClient::getResult, &client));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return fut;
}

// Test requests.
// We specify the "Connection: close" header so that the server will close the
// socket after transmitting the response. This will allow us to treat all data
// up until the EOF as the content.
const std::string GetRequest =
    "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";
// HTTTP
const std::string MalformadRequest =
    "GET /index.html HTTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";

}  // namespace

TEST_CASE("server should return binded port", "[server]") {
    asio::io_context ioc;
    http::server::Server s(ioc, "127.0.0.1", "0", ".");
    uint16_t port = s.getBindedPort();
    REQUIRE(port != 0);
}

TEST_CASE("server connection", "[server]") {
    asio::io_context ioc;
    http::server::Server s(ioc, "127.0.0.1", "0", ".");
    uint16_t port = s.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);
    TestClient c(ioc);

    SECTION("it should accept client connection") {
        auto fut = createFutureResult(c);
        c.connect("127.0.0.1", std::to_string(port), "/index.html");
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::Opened);
    }

    SECTION("it should return 404") {
        auto fut = createFutureResult(c);
        c.connect("127.0.0.1", std::to_string(port), "/index.html");
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::Opened);

        fut = createFutureResult(c);
        c.sendRequest(GetRequest);
        res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 404);
    }

    SECTION("it should return 400 for malformad requests") {
        auto fut = createFutureResult(c);
        c.connect("127.0.0.1", std::to_string(port), "/index.html");
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::Opened);

        fut = createFutureResult(c);
        c.sendRequest(MalformadRequest);
        res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 400);
    }

    ioc.stop();
    t.join();
    // std::raise(SIGTERM);
}
