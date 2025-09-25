#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <numeric>
#include <thread>

#include "utils/mock_file_io.hpp"
#include "utils/mock_not_found_handler.hpp"
#include "utils/mock_request_handler.hpp"
#include "utils/test_client.hpp"

#include "beauty/server.hpp"
#include "beauty/request_handler.hpp"

using namespace std::literals::chrono_literals;
using namespace beauty;

namespace {
enum class ExpectedResult { Connected, StatusLine, Headers, Content };
std::future<TestClient::TestResult> createFutureResult(
    TestClient& client,
    ExpectedResult expected = ExpectedResult::Content,
    size_t expectedContentLength = 0) {
    auto fut = std::async(std::launch::async, [&client, expected, expectedContentLength]() {
        switch (expected) {
            case ExpectedResult::Connected:
                return client.getConnectedResult(/*expectedContentLength*/);
            case ExpectedResult::StatusLine:
                return client.getStatusLineResult();
            case ExpectedResult::Headers:
                return client.getHeaderResult();
            case ExpectedResult::Content:
            default:
                return client.getContentResult(expectedContentLength);
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return fut;
}

TestClient::TestResult openConnection(TestClient& c, const std::string& host, uint16_t port) {
    auto fut = createFutureResult(c, ExpectedResult::Connected);
    c.connect(host, std::to_string(port));
    return fut.get();
}

std::vector<char> convertToCharVec(const std::vector<uint32_t> v) {
    std::vector<char> ret(v.size() * sizeof(decltype(v)::value_type));
    std::memcpy(ret.data(), v.data(), v.size() * sizeof(decltype(v)::value_type));
    return ret;
}
std::vector<char> convertToCharVec(const std::string& s) {
    std::vector<char> ret(s.begin(), s.end());
    return ret;
}

// Test requests.
// We specify the "Connection: close" header so that the server will close the
// socket after transmitting the response. This will allow us to treat all data
// up until the EOF as the content.
const std::string GetIndexRequest =
    "GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";

const std::string HeadIndexRequest =
    "HEAD /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";

const std::string GetUriWithQueryRequest =
    "GET /file.bin?myKey=myVal HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: "
    "keep-alive\r\n\r\n";

const std::string GetApiRequest =
    "GET /api/status HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";

}  // namespace

TEST_CASE("server should return binded port", "[server]") {
    asio::io_context ioc;
    HttpPersistence persistentOption(0s, 0, 0);
    Server s(ioc, "127.0.0.1", "0", persistentOption);
    uint16_t port = s.getBindedPort();
    REQUIRE(port != 0);
}

TEST_CASE("construction", "[server]") {
    SECTION("it should allow connection with simple constructor") {
        asio::io_context ioc;
        HttpPersistence persistentOption(0s, 0, 0);
        Server s(ioc, 0, persistentOption);
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
        HttpPersistence persistentOption(0s, 0, 0);
        Server s(ioc, 0, persistentOption);
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

    HttpPersistence persistentOption(0s, 0, 0);
    Server s(ioc, 0, persistentOption);
    uint16_t port = s.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should return 400 for malformad request") {
        // relative path not allowed
        const std::string malformadRequest =
            "GET ../index.html HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Accept: */*\r\n"
            "\r\n";
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        c.sendRequest(malformadRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.statusCode_ == 400);
    }
    SECTION("it should return 501 - not implemented for all valid requests") {
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.statusCode_ == 501);
    }
    SECTION("it should return version HTTP/1.1") {
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.httpVersion_ == "HTTP/1.1");
    }
    ioc.stop();
    t.join();
}

TEST_CASE("server with file handler", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    MockFileIO mockFileIO;
    HttpPersistence persistentOption(0s, 0, 0);
    Server dut(ioc, "127.0.0.1", "0", persistentOption);
    dut.setFileIO(&mockFileIO);
    uint16_t port = dut.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should return 404") {
        mockFileIO.setMockFailToOpenReadFile();
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.statusCode_ == 404);
    }

    SECTION("it should call fileIO openFileForRead but not close when no file exists") {
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        mockFileIO.setMockFailToOpenReadFile();
        c.sendRequest(GetIndexRequest);

        fut.get();
        REQUIRE(mockFileIO.getOpenFileForReadCalls() == 1);
        REQUIRE(mockFileIO.getCloseReadFileCalls() == 0);
    }

    SECTION("it should call fileIO openFileForRead and close when file exists") {
        openConnection(c, "127.0.0.1", port);
        mockFileIO.createMockFile(100);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        fut.get();
        REQUIRE(mockFileIO.getOpenFileForReadCalls() == 1);
        REQUIRE(mockFileIO.getCloseReadFileCalls() == 1);
    }

    SECTION("it should assume index.html for directories") {
        openConnection(c, "127.0.0.1", port);

        mockFileIO.createMockFile(100);
        const std::string getDirectoryRequest =
            "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";
        auto fut = createFutureResult(c);
        c.sendRequest(getDirectoryRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 100");
        REQUIRE(res.headers_[1] == "Content-Type: text/html");
        REQUIRE(res.headers_[2] == "Connection: close");
    }

    SECTION("it should return correct headers when no headers set in file handler") {
        openConnection(c, "127.0.0.1", port);
        mockFileIO.createMockFile(100);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        auto res = fut.get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 100");
        REQUIRE(res.headers_[1] == "Content-Type: text/html");
        REQUIRE(res.headers_[2] == "Connection: close");
    }

    SECTION("it should return correct headers when headers set in file handler") {
        openConnection(c, "127.0.0.1", port);
        mockFileIO.createMockFile(100);
        mockFileIO.addHeader({"X-Custom-Header", "MyValue"});
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        auto res = fut.get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.headers_.size() == 4);
        REQUIRE(res.headers_[0] == "X-Custom-Header: MyValue");
        REQUIRE(res.headers_[1] == "Content-Length: 100");
        REQUIRE(res.headers_[2] == "Content-Type: text/html");
        REQUIRE(res.headers_[3] == "Connection: close");
    }

    SECTION("it should return content less than chunk size") {
        openConnection(c, "127.0.0.1", port);

        const size_t fileSizeBytes = 100;
        mockFileIO.createMockFile(fileSizeBytes);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        std::vector<uint32_t> expectedContent(fileSizeBytes / sizeof(uint32_t));
        std::iota(expectedContent.begin(), expectedContent.end(), 0);
        REQUIRE(res.content_ == convertToCharVec(expectedContent));
    }

    SECTION("it should return content larger than chunk size") {
        openConnection(c, "127.0.0.1", port);

        const size_t fileSizeBytes = 10000;
        mockFileIO.createMockFile(fileSizeBytes);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        std::vector<uint32_t> expectedContent(fileSizeBytes / sizeof(uint32_t));
        std::iota(expectedContent.begin(), expectedContent.end(), 0);
        REQUIRE(res.content_ == convertToCharVec(expectedContent));
    }
    SECTION("it should return correct header for HEAD request") {
        openConnection(c, "127.0.0.1", port);

        mockFileIO.createMockFile(100);
        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(HeadIndexRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.statusCode_ == 200);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 100");
        REQUIRE(res.headers_[1] == "Content-Type: text/html");
        REQUIRE(res.headers_[2] == "Connection: close");
    }

    ioc.stop();
    t.join();
}

TEST_CASE("server with request handler", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    std::vector<char> buffer;
    MockRequestHandler mockRequestHandler(buffer);
    HttpPersistence persistentOption(0s, 0, 0);
    Server dut(ioc, "127.0.0.1", "0", persistentOption);
    uint16_t port = dut.getBindedPort();
    dut.addRequestHandler(std::bind(&MockRequestHandler::handleRequest,
                                    &mockRequestHandler,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should call request handler when defined") {
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(mockRequestHandler.getNoCalls() == 1);
    }
    SECTION("it should provide correct Request object") {
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(GetUriWithQueryRequest);

        auto res = fut.get();
        Request req = mockRequestHandler.getReceivedRequest();
        REQUIRE(req.method_ == "GET");
        REQUIRE(req.uri_ == "/file.bin?myKey=myVal");
        REQUIRE(req.httpVersionMajor_ == 1);
        REQUIRE(req.httpVersionMinor_ == 1);
        REQUIRE(req.headers_.size() == 3);
        REQUIRE(req.headers_[0].name_ == "Host");
        REQUIRE(req.headers_[0].value_ == "127.0.0.1");
        REQUIRE(req.headers_[1].name_ == "Accept");
        REQUIRE(req.headers_[1].value_ == "*/*");
        REQUIRE(req.headers_[2].name_ == "Connection");
        REQUIRE(req.headers_[2].value_ == "keep-alive");
        REQUIRE(req.body_.size() == 0);
    }
    SECTION("it should respond with status code") {
        mockRequestHandler.setReturnToClient(true);
        mockRequestHandler.setMockedReply(Reply::status_type::unauthorized, "some content");
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.statusCode_ == 401);
    }
    SECTION("it should respond with headers") {
        mockRequestHandler.setMockedReply(Reply::status_type::ok, "some content");
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);
        std::future<TestClient::TestResult> fut = createFutureResult(c);
        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.statusCode_ == 200);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 12");
        REQUIRE(res.headers_[1] == "Content-Type: text/plain");
        REQUIRE(res.headers_[2] == "Connection: close");
    }
    SECTION("it should respond with content") {
        std::string content = "this is some content";

        mockRequestHandler.setMockedReply(Reply::status_type::ok, content);
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);
        std::future<TestClient::TestResult> fut = createFutureResult(c);
        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.content_ == convertToCharVec(content));
    }
    SECTION("it should call all handlers if they return true") {
        std::vector<char> buffer2;  // not used in test
        MockRequestHandler mockRequestHandler2(buffer2);
        dut.addRequestHandler(std::bind(&MockRequestHandler::handleRequest,
                                        &mockRequestHandler2,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(mockRequestHandler.getNoCalls() == 1);
        REQUIRE(mockRequestHandler2.getNoCalls() == 1);
    }
    SECTION("it should only call all handlers until one returns false") {
        mockRequestHandler.setReturnToClient(true);
        std::vector<char> buffer2;  // not used in test
        MockRequestHandler mockRequestHandler2(buffer2);
        dut.addRequestHandler(std::bind(&MockRequestHandler::handleRequest,
                                        &mockRequestHandler2,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(mockRequestHandler.getNoCalls() == 1);
        REQUIRE(mockRequestHandler2.getNoCalls() == 0);
    }
    SECTION("it should return 413 when content exceeds max size") {
        mockRequestHandler.setReturnToClient(true);
        const std::string requestHeaders =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 1025\r\n\r\n";
        const std::string requestBody = "{ \"data\": \"" + std::string(1009, 'x') + "\" }\r\n";
        REQUIRE(requestBody.length() == 1025);

        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Content);
        c.sendRequest(requestHeaders + requestBody);

        auto res = fut.get();
        REQUIRE(mockRequestHandler.getNoCalls() == 0);
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.statusCode_ == 413);
    }

    ioc.stop();
    t.join();
}

TEST_CASE("server with write fileIO", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    MockFileIO mockFileIO;
    HttpPersistence persistentOption(0s, 0, 0);
    Server dut(ioc, "127.0.0.1", "0", persistentOption);
    dut.setFileIO(&mockFileIO);
    uint16_t port = dut.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should handle a complete multipart request") {
        // Note: It seems that clients typically post upto the end of each
        // multi-part header section, then make a new request for the file data
        // (First part). However this tests also checks that a complete post is
        // handled.
        const std::string requestHeaders =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 221\r\n\r\n";
        const std::string requestBody =
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "First part\n\r\n"
            "----------------------------338874100326900647006157--\r\n";
        REQUIRE(requestBody.length() == 221);

        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(requestHeaders + requestBody);
        auto res = fut.get();

        REQUIRE(res.statusCode_ == 201);  // MockFileIO::writeFile returns 201
        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 1);
        REQUIRE(mockFileIO.getLastData("/firstpart.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/firstpart.txt0");
        std::vector<char> expected = {'F', 'i', 'r', 's', 't', ' ', 'p', 'a', 'r', 't', '\n'};
        REQUIRE(result == expected);
    }
    SECTION("it should respond with fileIOs bad response") {
        const std::string requestHeaders =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 221\r\n\r\n";
        const std::string requestBody =
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "First part\n\r\n"
            "----------------------------338874100326900647006157--\r\n";
        REQUIRE(requestBody.length() == 221);

        mockFileIO.setMockFailToOpenWriteFile();

        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Content);
        c.sendRequest(requestHeaders + requestBody);
        auto res = fut.get();

        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.statusCode_ == 500);  // MockFileIO::openFileForWrite
        std::string expectedContent =
            "MockFileIO test error: simulated failure to open file for write";
        REQUIRE(res.content_ == convertToCharVec(expectedContent));
    }
    SECTION("it should handle multiple parts in a multipart request") {
        const std::string requestHeaders =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------383973011316738131928582\r\n"
            "Content-Length: 394\r\n\r\n";

        const std::string requestBody =
            "----------------------------383973011316738131928582\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "First part.\n\r\n"
            "----------------------------383973011316738131928582\r\n"
            "Content-Disposition: form-data; name=\"file2\"; filename=\"secondpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "Second part,\n\r\n----------------------------383973011316738131928582--\r\n";
        REQUIRE(requestBody.length() == 394);

        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(requestHeaders + requestBody);
        auto res = fut.get();

        REQUIRE(res.statusCode_ == 201);  // MockFileIO::openFileForWrite returns 201
        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 2);
        REQUIRE(mockFileIO.getLastData("/firstpart.txt0") == true);
        REQUIRE(mockFileIO.getLastData("/secondpart.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/firstpart.txt0");
        std::vector<char> expected = {'F', 'i', 'r', 's', 't', ' ', 'p', 'a', 'r', 't', '.', '\n'};
        REQUIRE(result == expected);
        result = mockFileIO.getMockWriteFile("/secondpart.txt0");
        expected = {'S', 'e', 'c', 'o', 'n', 'd', ' ', 'p', 'a', 'r', 't', ',', '\n'};
        REQUIRE(result == expected);
    }

    SECTION("it should handle large multipart request that exceeds buffer size") {
        // Create a multipart request with total size > 1024 bytes to test doReadBody chunked
        // reading
        const std::string boundary = "boundary123456789";  // Boundary without leading --

        // Create file content that makes total request > 1024 bytes
        std::string largeFileContent(1024, 'A');  // 1024 A's

        std::string requestBody =
            "--" + boundary +
            "\r\n"
            "Content-Disposition: form-data; name=\"largefile\"; filename=\"largefile.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n" +
            largeFileContent + "\r\n" + "--" + boundary + "--\r\n";

        std::string requestHeaders =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; boundary=" +
            boundary +
            "\r\n"
            "Content-Length: " +
            std::to_string(requestBody.length()) + "\r\n\r\n";

        // Verify total request size > 1024 to trigger chunked reading
        REQUIRE((requestHeaders + requestBody).length() > 1024);

        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(requestHeaders + requestBody);
        auto res = fut.get();

        REQUIRE(res.statusCode_ == 201);  // MockFileIO::openFileForWrite returns 201
        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 1);
        REQUIRE(mockFileIO.getLastData("/largefile.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/largefile.txt0");
        std::vector<char> expected(largeFileContent.begin(), largeFileContent.end());
        CHECK(result.size() == expected.size());
        REQUIRE(result == expected);
    }

    ioc.stop();
    t.join();
}

TEST_CASE("request handler with 100-continue support", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    std::vector<char> buffer;
    MockRequestHandler mockRequestHandler(buffer);
    HttpPersistence persistentOption(5s, 0, 0);
    Server dut(ioc, "127.0.0.1", "0", persistentOption);
    uint16_t port = dut.getBindedPort();

    dut.addRequestHandler(std::bind(&MockRequestHandler::handleRequest,
                                    &mockRequestHandler,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should call expect continue handler when set") {
        bool handlerCalled = false;
        std::string capturedMethod;

        dut.setExpectContinueHandler([&](const Request& req, Reply& rep) -> void {
            handlerCalled = true;
            capturedMethod = req.method_;
            rep.send(Reply::status_type::ok);  // Approve the request
        });

        const std::string request100Continue =
            "PUT /data HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Content-Length: 8\r\n"
            "Content-Type: text/plain\r\n"
            "Expect: 100-continue\r\n"
            "Connection: close\r\n\r\n";

        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);
        std::future<TestClient::TestResult> fut = createFutureResult(c, ExpectedResult::StatusLine);
        c.sendRequest(request100Continue);

        auto res = fut.get();
        REQUIRE(handlerCalled == true);
        REQUIRE(capturedMethod == "PUT");
        REQUIRE(res.statusCode_ == 100);
        REQUIRE(res.headers_.size() == 0);
    }

    SECTION("it should return rejected request from expect continue handler") {
        dut.setExpectContinueHandler([](const Request& /*req*/, Reply& rep) -> void {
            rep.addHeader("WWW-Authenticate", "Basic realm=\"Access to the site\"");
            rep.send(Reply::status_type::unauthorized);
        });

        const std::string request100Continue =
            "POST /upload HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Content-Length: 12\r\n"
            "Content-Type: application/json\r\n"
            "Expect: 100-continue\r\n\r\n";

        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(request100Continue);

        auto res = fut.get();
        // Should get 401 Unauthorized when rejected
        REQUIRE(res.statusCode_ == 401);
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "WWW-Authenticate: Basic realm=\"Access to the site\"");
    }
    SECTION("it should approve 100-continue when no handler is set") {
        // Default behavior should approve all 100-continue requests
        const std::string request100Continue =
            "POST /upload HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Content-Length: 12\r\n"
            "Content-Type: application/json\r\n"
            "Expect: 100-continue\r\n\r\n";

        const std::string requestBody = "Hello World!";

        mockRequestHandler.setMockedReply(Reply::status_type::created, "");
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);

        // Use the 100-continue method that handles the entire flow
        std::future<TestClient::TestResult> fut =
            std::async(std::launch::async, [&c]() { return c.getExpect100Result(); });

        c.sendRequestWithExpect100(request100Continue, requestBody);
        auto res = fut.get();

        REQUIRE(res.statusCode_ == 201);  // MockRequestHandler returns 201
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 0");  // Response body is empty
        REQUIRE(res.headers_[1] == "Connection: keep-alive");
        REQUIRE(res.headers_[2].find("Keep-Alive:") == 0);  // Keep-Alive header

        Request req = mockRequestHandler.getReceivedRequest();
        REQUIRE(req.method_ == "POST");
        REQUIRE(req.requestPath_ == "/upload");
        REQUIRE(req.body_.size() == 12);
        REQUIRE(std::string(req.body_.begin(), req.body_.end()) == "Hello World!");
    }
    SECTION("it should handle request without expect continue header normally") {
        bool handlerCalled = false;

        dut.setExpectContinueHandler([&](const Request& /*req*/, Reply& rep) -> void {
            handlerCalled = true;
            rep.send(Reply::status_type::ok);  // Approve the request
        });

        // Regular POST request without Expect: 100-continue
        const std::string regularRequest =
            "POST /upload HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Content-Length: 12\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n\r\n"
            "Hello World!";

        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c, ExpectedResult::Headers);
        c.sendRequest(regularRequest);
        auto res = fut.get();

        // Expect continue handler should not be called for regular requests
        REQUIRE(handlerCalled == false);
        REQUIRE(mockRequestHandler.getNoCalls() == 1);

        Request req = mockRequestHandler.getReceivedRequest();
        REQUIRE(req.expectsContinue() == false);
        REQUIRE(std::string(req.body_.begin(), req.body_.end()) == "Hello World!");
    }
    SECTION("it should return 413 when content exceeds max size for 100-continue") {
        mockRequestHandler.setReturnToClient(true);
        const std::string request100Continue =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: application/json\r\n"
            "Expect: 100-continue\r\n"
            "Content-Length: 1025\r\n\r\n";
        const std::string requestBody = "{ \"data\": \"" + std::string(1009, 'x') + "\" }\r\n";
        REQUIRE(requestBody.length() == 1025);

        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Content);
        c.sendRequestWithExpect100(request100Continue, requestBody);

        auto res = fut.get();
        REQUIRE(mockRequestHandler.getNoCalls() == 0);
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(res.statusCode_ == 413);
    }

    ioc.stop();
    t.join();
}

TEST_CASE("server with write fileIO with 100-continue support", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    MockFileIO mockFileIO;
    HttpPersistence persistentOption(5s, 0, 0);
    Server dut(ioc, "127.0.0.1", "0", persistentOption);
    dut.setFileIO(&mockFileIO);
    uint16_t port = dut.getBindedPort();

    // add a dummy that does nothing
    std::vector<char> buffer;
    MockRequestHandler mockRequestHandler(buffer);
    mockRequestHandler.setReturnToClient(false);
    dut.addRequestHandler(std::bind(&MockRequestHandler::handleRequest,
                                    &mockRequestHandler,
                                    std::placeholders::_1,
                                    std::placeholders::_2));

    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should handle a complete multipart request with expect 100-continue") {
        // Note: It seems that clients typically post upto the end of each
        // multi-part header section, then make a new request for the file data
        // (First part). However this tests also checks that a complete post is
        // handled.
        const std::string request100Continue =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 221\r\n"
            "Expect: 100-continue\r\n\r\n";
        const std::string requestBody =
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "First part\n\r\n"
            "----------------------------338874100326900647006157--\r\n";

        openConnection(c, "127.0.0.1", port);
        std::future<TestClient::TestResult> fut =
            std::async(std::launch::async, [&c]() { return c.getExpect100Result(); });
        c.sendMultiPartRequestWithExpect100(request100Continue, {requestBody});
        auto res = fut.get();

        REQUIRE(res.statusCode_ == 201);  // MockFileIO::writeFile returns 201
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 0");
        REQUIRE(res.headers_[1] == "Connection: keep-alive");
        REQUIRE(res.headers_[2].find("Keep-Alive:") == 0);

        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 1);
        REQUIRE(mockFileIO.getLastData("/firstpart.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/firstpart.txt0");
        std::vector<char> expected = {'F', 'i', 'r', 's', 't', ' ', 'p', 'a', 'r', 't', '\n'};
        REQUIRE(result == expected);
    }

    SECTION("it should handle large multipart request with expect 100-continue") {
        // Create a multipart request with total size > 1024 bytes to test
        // doReadBodyAfter100Continue chunked reading
        const std::string boundary = "boundary987654321";  // Boundary without leading --

        // Create file content that makes total request > 1024 bytes
        std::string largeFileContent(800, 'B');  // 800 B's

        std::string requestBody =
            "--" + boundary +
            "\r\n"
            "Content-Disposition: form-data; name=\"largefile\"; filename=\"largefile100.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n" +
            largeFileContent + "\r\n" + "--" + boundary + "--\r\n";

        std::string request100Continue =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; boundary=" +
            boundary +
            "\r\n"
            "Content-Length: " +
            std::to_string(requestBody.length()) +
            "\r\n"
            "Expect: 100-continue\r\n\r\n";

        // Verify total request size > 1024 to trigger chunked reading
        REQUIRE((request100Continue + requestBody).length() > 1024);

        openConnection(c, "127.0.0.1", port);
        std::future<TestClient::TestResult> fut =
            std::async(std::launch::async, [&c]() { return c.getExpect100Result(); });
        c.sendRequestWithExpect100(request100Continue, requestBody);
        auto res = fut.get();

        REQUIRE(res.statusCode_ == 201);  // MockFileIO::writeFile returns 201
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 0");
        REQUIRE(res.headers_[1] == "Connection: keep-alive");
        REQUIRE(res.headers_[2].find("Keep-Alive:") == 0);

        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 1);
        REQUIRE(mockFileIO.getLastData("/largefile100.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/largefile100.txt0");
        std::vector<char> expected(largeFileContent.begin(), largeFileContent.end());
        REQUIRE(result == expected);
    }
    SECTION("it should return 417 expected fail when client send body with 100-continue") {
        const std::string requestHeaders =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Expect: 100-continue\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 221\r\n\r\n";
        const std::string requestBody =
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "First part\n\r\n"
            "----------------------------338874100326900647006157--\r\n";
        REQUIRE(requestBody.length() == 221);

        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c, ExpectedResult::Content);
        // send a normal request with body instead of using 100-continue method
        c.sendRequest(requestHeaders + requestBody);
        auto res = fut.get();

        REQUIRE(res.statusCode_ == 417);  // Expectation Failed
    }

    ioc.stop();
    t.join();
}
