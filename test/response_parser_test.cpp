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

TEST_CASE("parse 1xx interim responses", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should skip a 100 Continue and parse the final response") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 100 Continue\r\n"
            "\r\n"
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "hello");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.statusCode_ == 200);
        REQUIRE(bodyAsString(fixture.response) == "hello");
    }

    SECTION("should skip an interim response that arrives before the final one") {
        auto result = fixture.parse_chunk("HTTP/1.1 100 Continue\r\n\r\n");
        REQUIRE(result == ResponseParser::good_part);
        result = fixture.parse_chunk(
            "HTTP/1.1 201 Created\r\n"
            "Content-Length: 2\r\n"
            "\r\n"
            "ok");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(fixture.response.statusCode_ == 201);
        REQUIRE(bodyAsString(fixture.response) == "ok");
    }
}

TEST_CASE("parse chunked transfer encoding", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should decode a simple chunked body") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\nhello\r\n"
            "6\r\n world\r\n"
            "0\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(bodyAsString(fixture.response) == "hello world");
    }

    SECTION("should ignore chunk extensions") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5;foo=bar\r\nhello\r\n"
            "0\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(bodyAsString(fixture.response) == "hello");
    }

    SECTION("should skip trailers after the last chunk") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "3\r\nabc\r\n"
            "0\r\n"
            "X-Trailer: value\r\n"
            "\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(bodyAsString(fixture.response) == "abc");
    }

    SECTION("should decode chunked body fed incrementally") {
        auto result = fixture.parse_chunk(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\nhel");
        REQUIRE(result == ResponseParser::good_part);
        result = fixture.parse_chunk("lo\r\n0\r\n\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(bodyAsString(fixture.response) == "hello");
    }

    SECTION("should decode hex chunk sizes larger than 9") {
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "a\r\n0123456789\r\n"
            "0\r\n\r\n");
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(bodyAsString(fixture.response) == "0123456789");
    }
}

TEST_CASE("parse body delimited by connection close", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("finish() completes a body with no Content-Length") {
        auto result = fixture.parse_complete(
            "HTTP/1.0 200 OK\r\n"
            "\r\n"
            "some body bytes");
        REQUIRE(result == ResponseParser::good_part);
        result = fixture.parser.finish(fixture.response);
        REQUIRE(result == ResponseParser::good_complete);
        REQUIRE(bodyAsString(fixture.response) == "some body bytes");
    }

    SECTION("finish() reports bad when closed mid-headers") {
        auto result = fixture.parse_complete("HTTP/1.1 200 OK\r\nContent-Len");
        REQUIRE(result == ResponseParser::good_part);
        result = fixture.parser.finish(fixture.response);
        REQUIRE(result == ResponseParser::bad);
    }
}

TEST_CASE("parse enforces max body size", "[response_parser]") {
    ResponseFixture fixture(1024);

    SECTION("should reject a Content-Length body that exceeds the limit") {
        fixture.parser.setMaxBodySize(4);
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 10\r\n"
            "\r\n"
            "0123456789");
        REQUIRE(result == ResponseParser::too_large);
    }

    SECTION("should reject a chunked body that exceeds the limit") {
        fixture.parser.setMaxBodySize(4);
        auto result = fixture.parse_complete(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "a\r\n0123456789\r\n"
            "0\r\n\r\n");
        REQUIRE(result == ResponseParser::too_large);
    }
}
