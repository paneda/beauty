#include <catch2/catch_test_macros.hpp>

#include <string>

#include "beauty/response.hpp"
#include "beauty/response_parser.hpp"

using namespace beauty;

namespace {

struct ResponseFixture {
    ResponseFixture(size_t maxContentSize) : response(body_) {
        recvBuffer_.reserve(maxContentSize);
        body_.reserve(maxContentSize);
    }

    // Feed the whole response at once.
    ResponseParser::result_type parse_complete(const std::string &text) {
        recvBuffer_.assign(text.begin(), text.end());
        return parser.parse(response, recvBuffer_);
    }

    // Feed an additional chunk of bytes (simulating multiple socket reads).
    ResponseParser::result_type parse_chunk(const std::string &text) {
        recvBuffer_.insert(recvBuffer_.end(), text.begin(), text.end());
        return parser.parse(response, recvBuffer_);
    }

    ResponseParser parser;
    std::vector<char> recvBuffer_;
    std::vector<char> body_;
    Response response;
};

std::string bodyAsString(const Response &res) {
    return std::string(res.body_.begin(), res.body_.end());
}

}  // namespace

TEST_CASE("parse status line", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should reject misspelled protocol") {
        auto result = fixture.parse_complete("HTTX/1.1 200 OK\r\n\r\n");
        REQUIRE(result == ResponseParser::bad);
    }

    SECTION("should parse 200 OK with no body") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.httpVersionMajor_ == 1);
        REQUIRE(fixture.response.httpVersionMinor_ == 1);
        REQUIRE(fixture.response.statusCode_ == 200);
        REQUIRE(fixture.response.statusMessage_ == "OK");
        REQUIRE(fixture.response.keepAlive_ == true);
    }

    SECTION("should parse reason phrase with spaces") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.statusCode_ == 404);
        REQUIRE(fixture.response.statusMessage_ == "Not Found");
    }

    SECTION("should parse missing reason phrase") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 204\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.statusCode_ == 204);
    }
}

TEST_CASE("parse headers", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should parse multiple headers") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n"
            "X-Custom: value\r\n"
            "\r\n"
            "hello");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.getHeaderValue("content-type") == "text/plain");
        REQUIRE(fixture.response.getHeaderValue("Content-Length") == "5");
        REQUIRE(fixture.response.getHeaderValue("X-Custom") == "value");
        REQUIRE(bodyAsString(fixture.response) == "hello");
    }

    SECTION("should honour Connection: close") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Connection: close\r\n"
            "Content-Length: 0\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.keepAlive_ == false);
    }
}

TEST_CASE("parse body by content length", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should read body of declared length") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
            "hello world");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.getContentLength() == 11);
        REQUIRE(bodyAsString(fixture.response) == "hello world");
    }

    SECTION("should leave trailing bytes in receive buffer") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "helloEXTRA");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(bodyAsString(fixture.response) == "hello");
        REQUIRE(std::string(fixture.recvBuffer_.begin(), fixture.recvBuffer_.end()) == "EXTRA");
    }
}

TEST_CASE("parse incrementally", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should resume across reads") {
        auto result = fixture.parse_chunk("HTTP/1.1 200 O");
        REQUIRE(result == ResponseParser::good_part);
        result = fixture.parse_chunk("K\r\nContent-Length: 5\r\n");
        REQUIRE(result == ResponseParser::good_part);
        result = fixture.parse_chunk("\r\nhel");
        REQUIRE(result == ResponseParser::good_part);
        result = fixture.parse_chunk("lo");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.statusCode_ == 200);
        REQUIRE(bodyAsString(fixture.response) == "hello");
    }
}

TEST_CASE("parse websocket upgrade", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should report switching_protocols for 101") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::switching_protocols);
        REQUIRE(fixture.response.statusCode_ == 101);
        REQUIRE(fixture.response.getHeaderValue("Upgrade") == "websocket");
        REQUIRE(fixture.response.getHeaderValue("Sec-WebSocket-Accept") ==
                "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    SECTION("should leave frame bytes after 101 in receive buffer") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "\r\n"
            "FRAMEDATA");
        REQUIRE(result == ResponseParser::switching_protocols);
        REQUIRE(std::string(fixture.recvBuffer_.begin(), fixture.recvBuffer_.end()) == "FRAMEDATA");
    }
}
