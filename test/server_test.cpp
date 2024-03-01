#include "server.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <csignal>
#include <future>
#include <numeric>
#include <thread>

#include "utils/mock_file_handler.hpp"
#include "utils/test_client.hpp"

using namespace std::literals::chrono_literals;
namespace {
std::future<TestClient::TestResult> createFutureResult(TestClient& client,
                                                       size_t expectedContentLength = 0) {
    auto fut = std::async(std::launch::async,
                          std::bind(&TestClient::getResult, &client, expectedContentLength));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return fut;
}

TestClient::TestResult openConnection(TestClient& c, const std::string& path, uint16_t port) {
    auto fut = createFutureResult(c);
    c.connect("127.0.0.1", std::to_string(port), path);
    return fut.get();
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
    MockFileHandler mockFileHandler;
    http::server::Server s(ioc, "127.0.0.1", "0", ".", mockFileHandler);
    uint16_t port = s.getBindedPort();
    REQUIRE(port != 0);
}

TEST_CASE("server connection", "[server]") {
    asio::io_context ioc;
    MockFileHandler mockFileHandler;
    http::server::Server s(ioc, "127.0.0.1", "0", ".", mockFileHandler);
    uint16_t port = s.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);
    TestClient c(ioc);

    SECTION("it should accept client connection") {
        auto res = openConnection(c, "/index.html", port);
        REQUIRE(res.action_ == TestClient::TestResult::Opened);
    }

    SECTION("it should return 400 for malformad requests") {
        openConnection(c, "/index.html", port);

        auto fut = createFutureResult(c);
        c.sendRequest(MalformadRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 400);
    }

    SECTION("it should return 404") {
        mockFileHandler.setMockFailToOpenFile();
        openConnection(c, "/index.html", port);

        auto fut = createFutureResult(c);
        c.sendRequest(GetRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 404);
    }

    SECTION("it should call fileHandler open/close") {
        openConnection(c, "/index.html", port);

        auto fut = createFutureResult(c);
        c.sendRequest(GetRequest);
        fut.get();
        REQUIRE(mockFileHandler.getOpenFileCalls() == 1);
        REQUIRE(mockFileHandler.getCloseFileCalls() == 1);
    }

    SECTION("it should return headers") {
        openConnection(c, "/index.html", port);

        mockFileHandler.createMockFile(100);
        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};
        c.sendRequest(GetRequest);
        futs[0].get();             // status
        auto res = futs[1].get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.headers_.size() == 2);
        REQUIRE(res.headers_[0] == "Content-Length: 100\r");
        REQUIRE(res.headers_[1] == "Content-Type: text/html\r");
    }

    SECTION("it should return content") {
        openConnection(c, "/index.html", port);

        const size_t fileSizeBytes = 10000;
        mockFileHandler.createMockFile(fileSizeBytes);
        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c), createFutureResult(c), createFutureResult(c, fileSizeBytes)};
        c.sendRequest(GetRequest);
        futs[0].get();             // status
        futs[1].get();             // headers
        auto res = futs[2].get();  // content
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        std::vector<uint32_t> expectedContent(fileSizeBytes / sizeof(uint32_t));
        std::iota(expectedContent.begin(), expectedContent.end(), 0);
        REQUIRE(res.content_ == expectedContent);
    }

    ioc.stop();
    t.join();
    // std::raise(SIGTERM);
}
