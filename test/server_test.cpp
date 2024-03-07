#include "server.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <csignal>
#include <future>
#include <numeric>
#include <thread>

#include "utils/mock_file_handler.hpp"
#include "utils/mock_route_handler.hpp"
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

TestClient::TestResult openConnection(TestClient& c, const std::string& host, uint16_t port) {
    auto fut = createFutureResult(c);
    c.connect(host, std::to_string(port));
    return fut.get();
}

std::vector<char> converToCharVec(std::vector<uint32_t> v) {
    std::vector<char> ret(v.size() * sizeof(decltype(v)::value_type));
    std::memcpy(ret.data(), v.data(), v.size() * sizeof(decltype(v)::value_type));
    return ret;
}

// Test requests.
// We specify the "Connection: close" header so that the server will close the
// socket after transmitting the response. This will allow us to treat all data
// up until the EOF as the content.
const std::string GetIndexRequest =
    "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";

const std::string GetApiRequest =
    "GET /api/status HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";
}  // namespace

TEST_CASE("server should return binded port", "[server]") {
    asio::io_context ioc;
    http::server::Server s(ioc, "127.0.0.1", "0", "", nullptr, "", nullptr);
    uint16_t port = s.getBindedPort();
    REQUIRE(port != 0);
}

TEST_CASE("contruction", "[server]") {
    SECTION("it should allow connection with simple constructor") {
        asio::io_context ioc;
        http::server::Server s(ioc, 0, "", nullptr, "", nullptr);
        uint16_t port = s.getBindedPort();
        REQUIRE(port != 0);

        auto t = std::thread(&asio::io_context::run, &ioc);
        TestClient c(ioc);
        auto res = openConnection(c, "127.0.0.1", port);
        REQUIRE(res.action_ == TestClient::TestResult::Opened);

        ioc.stop();
        t.join();
    }
    SECTION("it should allow connection with advanced constructor", "[server]") {
        asio::io_context ioc;
        http::server::Server s(ioc, "127.0.0.1", "0", "", nullptr, "", nullptr);
        uint16_t port = s.getBindedPort();
        REQUIRE(port != 0);

        auto t = std::thread(&asio::io_context::run, &ioc);
        TestClient c(ioc);
        auto res = openConnection(c, "127.0.0.1", port);
        REQUIRE(res.action_ == TestClient::TestResult::Opened);

        ioc.stop();
        t.join();
    }
}

TEST_CASE("server without handlers", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    MockFileHandler mockFileHandler;
    http::server::Server s(ioc, "127.0.0.1", "0", "", nullptr, "", nullptr);
    uint16_t port = s.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should return 400 for mispelling") {
        // HTTTP spelled incorrectly
        const std::string malformadRequest =
            "GET /index.html HTTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: "
            "close\r\n\r\n";
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendRequest(malformadRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 400);
    }
    SECTION("it should return 400 for wrong path") {
        // HTTTP spelled incorrectly
        const std::string malformadRequest =
            "GET ../index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: "
            "*/*\r\nConnection: "
            "close\r\n\r\n";
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendRequest(malformadRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 400);
    }
    SECTION("it should return 404 for all valid requests") {
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 404);
    }
    ioc.stop();
    t.join();
}

TEST_CASE("server with file handler", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    MockFileHandler mockFileHandler;
    http::server::Server s(ioc, "127.0.0.1", "0", ".", &mockFileHandler, "", nullptr);
    uint16_t port = s.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should return 404") {
        mockFileHandler.setMockFailToOpenRequestedFile();
        mockFileHandler.setMockFailToOpenGzFile();
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 404);
    }

    SECTION("it should call fileHandler open/close") {
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);
        fut.get();
        REQUIRE(mockFileHandler.getOpenFileCalls() == 1);
        REQUIRE(mockFileHandler.getCloseFileCalls() == 1);
    }

    SECTION("it should return correct headers when gz file not found") {
        openConnection(c, "127.0.0.1", port);

        mockFileHandler.createMockFile(100);
        mockFileHandler.setMockFailToOpenGzFile();
        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};
        c.sendRequest(GetIndexRequest);
        futs[0].get();             // status
        auto res = futs[1].get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.headers_.size() == 2);
        REQUIRE(res.headers_[0] == "Content-Length: 100\r");
        REQUIRE(res.headers_[1] == "Content-Type: text/html\r");
    }

    SECTION("it should return correct headers when gz file is found") {
        openConnection(c, "127.0.0.1", port);

        mockFileHandler.createMockFile(100);
        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};
        c.sendRequest(GetIndexRequest);
        futs[0].get();             // status
        auto res = futs[1].get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 100\r");
        REQUIRE(res.headers_[1] == "Content-Type: text/html\r");
        REQUIRE(res.headers_[2] == "Content-Encoding: gzip\r");
    }

    SECTION("it should return content less than chunk size") {
        openConnection(c, "127.0.0.1", port);

        const size_t fileSizeBytes = 100;
        mockFileHandler.createMockFile(fileSizeBytes);
        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c), createFutureResult(c), createFutureResult(c, fileSizeBytes)};
        c.sendRequest(GetIndexRequest);
        futs[0].get();             // status
        futs[1].get();             // headers
        auto res = futs[2].get();  // content
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        std::vector<uint32_t> expectedContent(fileSizeBytes / sizeof(uint32_t));
        std::iota(expectedContent.begin(), expectedContent.end(), 0);
        REQUIRE(res.content_ == converToCharVec(expectedContent));
    }

    SECTION("it should return content larger than chunk size") {
        openConnection(c, "127.0.0.1", port);

        const size_t fileSizeBytes = 10000;
        mockFileHandler.createMockFile(fileSizeBytes);
        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c), createFutureResult(c), createFutureResult(c, fileSizeBytes)};
        c.sendRequest(GetIndexRequest);
        futs[0].get();             // status
        futs[1].get();             // headers
        auto res = futs[2].get();  // content
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        std::vector<uint32_t> expectedContent(fileSizeBytes / sizeof(uint32_t));
        std::iota(expectedContent.begin(), expectedContent.end(), 0);
        REQUIRE(res.content_ == converToCharVec(expectedContent));
    }

    ioc.stop();
    t.join();
}

TEST_CASE("server with route handler", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    http::server::MockRouteHandler mockRouteHandler;
    http::server::Server s(ioc, "127.0.0.1", "0", "", nullptr, "/api", &mockRouteHandler);
    uint16_t port = s.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should respond with 500 when route handler not implemented") {
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);

        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 500);
    }

    SECTION("it should respond with status code") {
        mockRouteHandler.setMockedReponseStatus(http::server::Reply::status_type::not_found);
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);

        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 404);
    }
    SECTION("it should respond with headers") {
        std::string content = "this is some content";
        std::vector<char> expectedContent(content.begin(), content.end());

        mockRouteHandler.setMockedReponseStatus(http::server::Reply::status_type::ok);
        mockRouteHandler.setMockedContent(content);
        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};

        c.sendRequest(GetApiRequest);

        auto res = futs[0].get();  // status
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 200);
        res = futs[1].get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.headers_.size() == 2);
        REQUIRE(res.headers_[0] == "Content-Length: 20\r");
        REQUIRE(res.headers_[1] == "Content-Type: application/json\r");
    }
    SECTION("it should respond with content") {
        std::string content = "this is some content";
        std::vector<char> expectedContent(content.begin(), content.end());

        mockRouteHandler.setMockedReponseStatus(http::server::Reply::status_type::ok);
        mockRouteHandler.setMockedContent(content);
        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c),
            createFutureResult(c),
            createFutureResult(c, expectedContent.size())};

        c.sendRequest(GetApiRequest);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        futs[0].get();  // status
        futs[1].get();  // headers
        auto res = futs[2].get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.content_ == expectedContent);
    }

    ioc.stop();
    t.join();
}
