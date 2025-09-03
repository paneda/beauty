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

const std::string GetUriWithQueryRequest =
    "GET /file.bin?myKey=myVal HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: "
    "keep-alive\r\n\r\n";

const std::string GetApiRequest =
    "GET /api/status HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";

}  // namespace

TEST_CASE("server should return binded port", "[server]") {
    asio::io_context ioc;
    HttpPersistence persistentOption(0s, 0, 0);
    Server s(ioc, "127.0.0.1", "0", nullptr, persistentOption);
    uint16_t port = s.getBindedPort();
    REQUIRE(port != 0);
}

TEST_CASE("contruction", "[server]") {
    SECTION("it should allow connection with simple constructor") {
        asio::io_context ioc;
        HttpPersistence persistentOption(0s, 0, 0);
        Server s(ioc, 0, nullptr, persistentOption);
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
        Server s(ioc, 0, nullptr, persistentOption);
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
    Server s(ioc, 0, nullptr, persistentOption);
    uint16_t port = s.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should return 400 for malformad request") {
        // relative path not allowed
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
    SECTION("it should return 501 - not implemented for all valid requests") {
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 501);
    }
    SECTION("it should return version HTTP/1.1") {
        openConnection(c, "127.0.0.1", port);
        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
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
    Server dut(ioc, "127.0.0.1", "0", &mockFileIO, persistentOption);
    uint16_t port = dut.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should return 404") {
        mockFileIO.setMockFailToOpenReadFile();
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendRequest(GetIndexRequest);
        auto res = fut.get();
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 404);
    }

    SECTION("it should call fileIO openFileForRead but not close when no file exists") {
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
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
        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};
        const std::string getDirectoryRequest =
            "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: */*\r\nConnection: close\r\n\r\n";
        c.sendRequest(getDirectoryRequest);
        futs[0].get();             // status
        auto res = futs[1].get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 100\r");
        REQUIRE(res.headers_[1] == "Content-Type: text/html\r");
        REQUIRE(res.headers_[2] == "Connection: close\r");
    }

    SECTION("it should return correct headers") {
        openConnection(c, "127.0.0.1", port);

        mockFileIO.createMockFile(100);
        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};
        c.sendRequest(GetIndexRequest);
        futs[0].get();             // status
        auto res = futs[1].get();  // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 100\r");
        REQUIRE(res.headers_[1] == "Content-Type: text/html\r");
        REQUIRE(res.headers_[2] == "Connection: close\r");
    }

    SECTION("it should return content less than chunk size") {
        openConnection(c, "127.0.0.1", port);

        const size_t fileSizeBytes = 100;
        mockFileIO.createMockFile(fileSizeBytes);
        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c), createFutureResult(c), createFutureResult(c, fileSizeBytes)};
        c.sendRequest(GetIndexRequest);
        futs[0].get();             // status
        futs[1].get();             // headers
        auto res = futs[2].get();  // content
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        std::vector<uint32_t> expectedContent(fileSizeBytes / sizeof(uint32_t));
        std::iota(expectedContent.begin(), expectedContent.end(), 0);
        REQUIRE(res.content_ == convertToCharVec(expectedContent));
    }

    SECTION("it should return content larger than chunk size") {
        openConnection(c, "127.0.0.1", port);

        const size_t fileSizeBytes = 10000;
        mockFileIO.createMockFile(fileSizeBytes);
        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c), createFutureResult(c), createFutureResult(c, fileSizeBytes)};
        c.sendRequest(GetIndexRequest);
        futs[0].get();             // status
        futs[1].get();             // headers
        auto res = futs[2].get();  // content
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        std::vector<uint32_t> expectedContent(fileSizeBytes / sizeof(uint32_t));
        std::iota(expectedContent.begin(), expectedContent.end(), 0);
        REQUIRE(res.content_ == convertToCharVec(expectedContent));
    }
    SECTION("it should return reply from fileNotFoundHandler") {
        std::string mockedContent = "This is mocked content";
        MockNotFoundHandler mockNotFoundHandler;
        mockNotFoundHandler.setMockedContent(mockedContent);
        mockFileIO.setMockFailToOpenReadFile();
        dut.setFileNotFoundHandler(std::bind(&MockNotFoundHandler::handleNotFound,
                                             &mockNotFoundHandler,
                                             std::placeholders::_1,
                                             std::placeholders::_2));
        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[3] = {createFutureResult(c),
                                                       createFutureResult(c),
                                                       createFutureResult(c, mockedContent.size())};

        c.sendRequest(GetIndexRequest);

        futs[0].get();             // status
        futs[1].get();             // headers
        auto res = futs[2].get();  // content
        REQUIRE(res.action_ == TestClient::TestResult::ReadContent);
        REQUIRE(mockNotFoundHandler.getNoCalls() == 1);
        REQUIRE(res.statusCode_ == 200);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 22\r");
        REQUIRE(res.headers_[1] == "Content-Type: text/plain\r");
        REQUIRE(res.headers_[2] == "Connection: close\r");
        REQUIRE(res.content_ == convertToCharVec(mockedContent));
    }

    ioc.stop();
    t.join();
}

TEST_CASE("server with route handler", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    std::vector<char> buffer;
    MockRequestHandler mockRequestHandler(buffer);
    HttpPersistence persistentOption(0s, 0, 0);
    Server dut(ioc, "127.0.0.1", "0", nullptr, persistentOption);
    uint16_t port = dut.getBindedPort();
    dut.addRequestHandler(std::bind(&MockRequestHandler::handleRequest,
                                    &mockRequestHandler,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should call request handler when defined") {
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);

        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(mockRequestHandler.getNoCalls() == 1);
    }
    SECTION("it should provide correct Request object") {
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);

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
        REQUIRE(res.action_ == TestClient::TestResult::ReadRequestStatus);
        REQUIRE(res.statusCode_ == 401);
    }
    SECTION("it should respond with headers") {
        mockRequestHandler.setMockedReply(Reply::status_type::ok, "some content");
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};

        c.sendRequest(GetApiRequest);

        auto res = futs[0].get();  // status
        res = futs[1].get();       // headers
        REQUIRE(res.action_ == TestClient::TestResult::ReadHeaders);
        REQUIRE(res.statusCode_ == 200);
        REQUIRE(res.headers_.size() == 3);
        REQUIRE(res.headers_[0] == "Content-Length: 12\r");
        REQUIRE(res.headers_[1] == "Content-Type: text/plain\r");
        REQUIRE(res.headers_[2] == "Connection: close\r");
    }
    SECTION("it should respond with content") {
        std::string content = "this is some content";

        mockRequestHandler.setMockedReply(Reply::status_type::ok, content);
        mockRequestHandler.setReturnToClient(true);
        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c), createFutureResult(c), createFutureResult(c, content.size())};

        c.sendRequest(GetApiRequest);

        futs[0].get();  // status
        futs[1].get();  // headers
        auto res = futs[2].get();
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

        auto fut = createFutureResult(c);

        c.sendRequest(GetApiRequest);

        auto res = fut.get();
        REQUIRE(mockRequestHandler.getNoCalls() == 1);
        REQUIRE(mockRequestHandler2.getNoCalls() == 0);
    }

    ioc.stop();
    t.join();
}

TEST_CASE("server with write fileIO", "[server]") {
    asio::io_context ioc;
    TestClient c(ioc);

    std::vector<char> buffer;  // not used in test
    MockRequestHandler mockRequestHandler(buffer);
    MockFileIO mockFileIO;
    HttpPersistence persistentOption(0s, 0, 0);
    Server dut(ioc, "127.0.0.1", "0", &mockFileIO, persistentOption);
    uint16_t port = dut.getBindedPort();
    auto t = std::thread(&asio::io_context::run, &ioc);

    SECTION("it should handle a complete multipart request") {
        // Note: It seems that clients typically post upto the end of each
        // multi-part header section, then make a new request for the file data
        // (First part). However this tests also checks that a complete post is
        // handled.
        const std::string request =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 222\r\n\r\n"
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "First part\n\r\n"
            "----------------------------338874100326900647006157--\r\n";

        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendMultiPartRequest({request});
        auto res = fut.get();
        REQUIRE(res.statusCode_ == 200);  // MockFileIO::writeFile returns 200
        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 1);
        REQUIRE(mockFileIO.getLastData("/firstpart.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/firstpart.txt0");
        std::vector<char> expected = {'F', 'i', 'r', 's', 't', ' ', 'p', 'a', 'r', 't', '\n'};
        REQUIRE(result == expected);
    }
    SECTION("it should handle divided multipart request with request headers only") {
        const std::string request1 =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 222\r\n\r\n";
        const std::string request2 =
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "First part\n\r\n----------------------------338874100326900647006157--\r\n";

        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};
        c.sendMultiPartRequest({request1, request2});
        auto res1 = futs[0].get();
        auto res2 = futs[1].get();

        REQUIRE(res1.statusCode_ == 200);  // Header response
        REQUIRE(res2.statusCode_ == 200);  // MockFileIO::writeFile returns 200
        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 1);
        REQUIRE(mockFileIO.getLastData("/firstpart.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/firstpart.txt0");
        std::vector<char> expected = {'F', 'i', 'r', 's', 't', ' ', 'p', 'a', 'r', 't', '\n'};
        REQUIRE(result == expected);
    }
    SECTION("it should handle divided multipart request that includes initial part headers") {
        const std::string request1 =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 222\r\n\r\n"
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n";
        const std::string request2 =
            "First part.\n\r\n----------------------------338874100326900647006157--\r\n";

        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[2] = {createFutureResult(c),
                                                       createFutureResult(c)};
        c.sendMultiPartRequest({request1, request2});
        auto res1 = futs[0].get();
        auto res2 = futs[1].get();
        REQUIRE(res1.statusCode_ == 201);  // MockFileIO::openFileForWrite
                                           // returns 201

        REQUIRE(res2.statusCode_ == 200);  // MockFileIO::writeFile returns 200
        REQUIRE(mockFileIO.getOpenFileForWriteCalls() == 1);
        REQUIRE(mockFileIO.getLastData("/firstpart.txt0") == true);
        std::vector<char> result = mockFileIO.getMockWriteFile("/firstpart.txt0");
        std::vector<char> expected = {'F', 'i', 'r', 's', 't', ' ', 'p', 'a', 'r', 't', '.', '\n'};
        REQUIRE(result == expected);
    }
    SECTION("it should respond with fileIOs bad response") {
        const std::string request1 =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------338874100326900647006157\r\n"
            "Content-Length: 222\r\n\r\n"
            "--------------------------338874100326900647006157\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n";

        mockFileIO.setMockFailToOpenWriteFile();

        openConnection(c, "127.0.0.1", port);

        auto fut = createFutureResult(c);
        c.sendMultiPartRequest({request1});
        auto res = fut.get();
        REQUIRE(res.statusCode_ == 500);  // MockFileIO::openFileForWrite
    }
    SECTION("it should handle multiple parts in a multipart request") {
        const std::string request1 =
            "POST / HTTP/1.1\r\n"
            "Host: 127.0.0.1:8081\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: multipart/form-data; "
            "boundary=--------------------------383973011316738131928582\r\n"
            "Content-Length: 394\r\n\r\n"
            "----------------------------383973011316738131928582\r\n"
            "Content-Disposition: form-data; name=\"file1\"; filename=\"firstpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n";
        const std::string request2 =
            "First part.\n\r\n"
            "----------------------------383973011316738131928582\r\n"
            "Content-Disposition: form-data; name=\"file2\"; filename=\"secondpart.txt\"\r\n"
            "Content-Type: text/plain\r\n\r\n";
        const std::string request3 =
            "Second part,\n\r\n----------------------------383973011316738131928582--\r\n";

        openConnection(c, "127.0.0.1", port);

        std::future<TestClient::TestResult> futs[3] = {
            createFutureResult(c), createFutureResult(c), createFutureResult(c)};
        c.sendMultiPartRequest({request1, request2, request3});
        auto res1 = futs[0].get();
        auto res2 = futs[1].get();
        auto res3 = futs[2].get();

        REQUIRE(res1.statusCode_ == 201);  // MockFileIO::openFileForWrite returns 201
        REQUIRE(res2.statusCode_ == 201);  // MockFileIO::openFileForWrite returns 201
        REQUIRE(res3.statusCode_ == 200);  // MockFileIO::writeFile returns 200
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

    ioc.stop();
    t.join();
}
