#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "request.hpp"
#include "request_parser.hpp"

using namespace http::server;

namespace {

std::vector<char> convertToCharVec(const std::string &s) {
    std::vector<char> ret(s.begin(), s.end());
    return ret;
}

struct RequestFixture {
    RequestFixture(size_t maxContentSize) {
        content_.clear();
        content_.reserve(maxContentSize);
    }
    RequestParser::result_type parse(const std::string &text) {
        RequestParser parser;
        content_.insert(content_.begin(), text.begin(), text.begin() + content_.capacity());
        return parser.parse(request, content_);
    }
    Request request;
    std::vector<char> content_;
};

}  // namespace

TEST_CASE("parse GET request", "[request_parser]") {
    RequestFixture fixture(1024);
    SECTION("should return false for misspelling") {
        const std::string request = "GET /uri HTTTP/0.9\r\n\r\n";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should parse GET HTTP/0.9") {
        const std::string request = "GET /uri HTTP/0.9\r\n\r\n";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 0);
        REQUIRE(fixture.request.httpVersionMinor_ == 9);
    }
    SECTION("should parse GET HTTP/1.0") {
        const std::string request = "GET /uri HTTP/1.0\r\n\r\n";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 0);
        REQUIRE(fixture.request.keepAlive_ == false);
    }
    SECTION("should parse GET HTTP/1.1") {
        const std::string request = "GET /uri HTTP/1.1\r\n\r\n";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 1);
        REQUIRE(fixture.request.keepAlive_);
    }
    SECTION("should parse uri with query params") {
        const std::string request = "GET /uri?arg1=test&arg1=%20%21&arg3=test\r\n\r\n";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri?arg1=test&arg1=%20%21&arg3=test");
    }
}

TEST_CASE("parse POST request", "[request_parser]") {
    RequestFixture fixture(1024);
    SECTION("should parse POST HTTP/1.1") {
        const std::string request = "POST /uri HTTP/1.1\r\n\r\n";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "POST");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 1);
        REQUIRE(fixture.request.keepAlive_);
    }
    SECTION("should parse POST HTTP/1.1 with header field") {
        const std::string request =
            "POST /uri HTTP/1.1\r\n"
            "X-Custom-Header: header value\r\n"
            "\r\n";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.headers_.size() == 1);
        REQUIRE(fixture.request.headers_[0].name_ == "X-Custom-Header");
        REQUIRE(fixture.request.headers_[0].value_ == "header value");
    }
    SECTION("should parse POST HTTP/1.1 with body") {
        const std::string request =
            "POST /uri.cgi HTTP/1.1\r\n"
            "From: user@example.com\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:18.0) Gecko/20100101 "
            "Firefox/18.0\r\n"
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: 31\r\n"
            "\r\n"
            "arg1=test;arg1=%20%21;arg3=test";

        auto result = fixture.parse(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.headers_.size() == 6);
        REQUIRE(fixture.request.headers_[0].name_ == "From");
        REQUIRE(fixture.request.headers_[0].value_ == "user@example.com");
        REQUIRE(fixture.request.headers_[1].name_ == "User-Agent");
        REQUIRE(fixture.request.headers_[1].value_ ==
                "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:18.0) Gecko/20100101 Firefox/18.0");
        REQUIRE(fixture.request.headers_[2].name_ == "Accept");
        REQUIRE(fixture.request.headers_[2].value_ ==
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        REQUIRE(fixture.request.headers_[3].name_ == "Accept-Encoding");
        REQUIRE(fixture.request.headers_[3].value_ == "gzip, deflate");
        REQUIRE(fixture.request.headers_[4].name_ == "Content-Type");
        REQUIRE(fixture.request.headers_[4].value_ == "application/x-www-form-urlencoded");
        REQUIRE(fixture.request.headers_[5].name_ == "Content-Length");
        REQUIRE(fixture.request.headers_[5].value_ == "31");
        std::vector<char> expectedContent = convertToCharVec("arg1=test;arg1=%20%21;arg3=test");
        REQUIRE(fixture.content_ == expectedContent);
        REQUIRE(fixture.request.getNoInitialBodyBytesReceived() == expectedContent.size());
    }
    SECTION("should return bad for POST HTTP/1.1 with chunked body") {
        const std::string request =
            "POST /uri.cgi HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "24\r\n"
            "This is the data in the first chunk \r\n"
            "1B\r\n"
            "and this is the second one \r\n"
            "3\r\n"
            "con\r\n"
            "9\r\n"
            "sequence\0\r\n"
            "0\r\n\r\n";

        auto result = fixture.parse(request);
        REQUIRE(result == RequestParser::bad);
    }
}

TEST_CASE("parse POST request partially", "[request_parser]") {
    RequestFixture fixture(320);
    const std::string headers =
        "POST / HTTP/1.1\r\n"
        "From: user@example.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/122.0.0.0 Safari/537.36\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Content-Type: multipart/form-data; "
        "boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Length: 420\r\n"
        "\r\n";
    const std::string body =
        "This request includes headers and some body data (this text) that does not fit the input "
        "content buffer of 320 bytes.";
    "\r\n";

    auto result = fixture.parse(headers + body);

    REQUIRE(result == RequestParser::good_part);
    std::vector<char> expectedContent = convertToCharVec("This request");
    REQUIRE(fixture.content_ == expectedContent);
    REQUIRE(fixture.request.getNoInitialBodyBytesReceived() == expectedContent.size());
}
