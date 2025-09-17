#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "beauty/request.hpp"
#include "beauty/request_parser.hpp"

using namespace beauty;

namespace {

std::vector<char> convertToCharVec(const std::string &s) {
    std::vector<char> ret(s.begin(), s.end());
    return ret;
}

struct RequestFixture {
    RequestFixture(size_t maxContentSize) : request(content_) {
        content_.clear();
        content_.reserve(maxContentSize);
    }
    // method to use when the entire request is available
    RequestParser::result_type parse_complete(const std::string &text) {
        RequestParser parser;
        content_.insert(content_.begin(), text.begin(), text.end());
        return parser.parse(request, content_);
    }
    // method to use when only part of the request is available
    RequestParser::result_type parse_partially(const std::string &text) {
        RequestParser parser;
        content_.insert(content_.begin(), text.begin(), text.begin() + content_.capacity());
        return parser.parse(request, content_);
    }
    std::vector<char> content_;
    Request request;
};

}  // namespace

TEST_CASE("parse GET request", "[request_parser]") {
    RequestFixture fixture(1024);
    SECTION("should return false for misspelling") {
        const std::string request = "GET /uri HTTTP/0.9\r\n\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should parse GET HTTP/1.0") {
        const std::string request = "GET /uri HTTP/1.0\r\nHost: www.example.com\r\n\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 0);
        REQUIRE(fixture.request.keepAlive_ == false);
    }
    SECTION("should parse GET HTTP/1.0 with Connection: Keep-Alive") {
        const std::string request =
            "GET /uri HTTP/1.0\r\n"
            "Connection: Keep-Alive\r\n"
            "Host: www.example.com\r\n"
            "\r\n";
        auto result = fixture.parse_complete(request);
        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 0);
        REQUIRE(fixture.request.keepAlive_ == true);
    }
    SECTION("should parse GET HTTP/1.1") {
        const std::string request =
            "GET /uri HTTP/1.1\r\n"
            "Host: www.example.com\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 1);
        REQUIRE(fixture.request.keepAlive_);
    }
    SECTION("should parse GET HTTP/1.1 with Connection: close") {
        const std::string request =
            "GET /uri HTTP/1.1\r\n"
            "Connection: close\r\n"
            "Host: www.example.com\r\n"
            "\r\n";
        auto result = fixture.parse_complete(request);
        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 1);
        REQUIRE(fixture.request.keepAlive_ == false);
    }
    SECTION("should parse uri with query params") {
        const std::string request =
            "GET /uri?arg1=test&arg1=%20%21&arg3=test HTTP/1.0\r\n"
            "Connection: Keep-Alive\r\n"
            "Host: www.example.com\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri?arg1=test&arg1=%20%21&arg3=test");
    }
}

TEST_CASE("version handling", "[request_parser]") {
    RequestFixture fixture(1024);

    SECTION("should respond with 505, version not supported, for higher version requests") {
        const std::string request =
            "GET /uri HTTP/2.0\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::version_not_supported);
    }
}

TEST_CASE("parse POST request", "[request_parser]") {
    RequestFixture fixture(1024);
    SECTION("should parse POST HTTP/1.1") {
        const std::string request =
            "POST /uri HTTP/1.1\r\n"
            "Host: www.example.com\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

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
            "Host: www.example.com\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.headers_.size() == 3);
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
            "Host: www.example.com\r\n"
            "\r\n"
            "arg1=test;arg1=%20%21;arg3=test";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.headers_.size() == 7);
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
        REQUIRE(fixture.request.headers_[6].name_ == "Host");
        REQUIRE(fixture.request.headers_[6].value_ == "www.example.com");
        std::vector<char> expectedContent = convertToCharVec("arg1=test;arg1=%20%21;arg3=test");
        REQUIRE(fixture.content_ == expectedContent);
        REQUIRE(fixture.request.getNoInitialBodyBytesReceived() == expectedContent.size());
        REQUIRE(fixture.request.body_ == expectedContent);
    }
    SECTION("should return missing_content_length POST HTTP/1.1 without Content-Length") {
        const std::string request =
            "POST /uri.cgi HTTP/1.1\r\n"
            "From: user@example.com\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:18.0) Gecko/20100101 "
            "Firefox/18.0\r\n"
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Host: www.example.com\r\n"
            "\r\n"
            "arg1=test;arg1=%20%21;arg3=test";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::missing_content_length);
    }
    SECTION("should return missing_content_length for POST HTTP/1.1 with chunked body") {
        const std::string request =
            "POST /uri.cgi HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Host: www.example.com\r\n"
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

        auto result = fixture.parse_complete(request);
        REQUIRE(result == RequestParser::missing_content_length);
    }
    SECTION("should return bad POST HTTP/1.1 with chunked body and Content-Length") {
        const std::string request =
            "POST /uri.cgi HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Host: www.example.com\r\n"
            "Content-Length: 100\r\n"
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

        auto result = fixture.parse_complete(request);
        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should return good_complete for POST HTTP/1.0 without Content-Length") {
        const std::string request =
            "POST /uri.cgi HTTP/1.0\r\n"
            "Content-Type: text/plain\r\n"
            "Host: www.example.com\r\n"
            "\r\n"
            "some content";

        auto result = fixture.parse_complete(request);
        REQUIRE(result == RequestParser::good_complete);
        REQUIRE(fixture.request.method_ == "POST");
        REQUIRE(fixture.request.uri_ == "/uri.cgi");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 0);
        REQUIRE(fixture.request.keepAlive_ == false);
        REQUIRE(fixture.content_ == convertToCharVec("some content"));
    }
}

TEST_CASE("parse HTTP/1.1 POST request partially", "[request_parser]") {
    RequestFixture fixture(343);
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
        "Host: www.example.com\r\n"
        "\r\n";
    const std::string body =
        "This request includes headers and some body data (this text) that does not fit the input "
        "content buffer of 343 bytes.\r\n";

    auto result = fixture.parse_partially(headers + body);

    REQUIRE(result == RequestParser::good_part);
    std::vector<char> expectedContent = convertToCharVec("This request");
    REQUIRE(fixture.content_ == expectedContent);
    REQUIRE(fixture.request.getNoInitialBodyBytesReceived() == expectedContent.size());
    REQUIRE(fixture.request.body_ == expectedContent);
}

TEST_CASE("parse HTTP/1.0 POST request partially without Content-Length", "[request_parser]") {
    RequestFixture fixture(322);
    const std::string headers =
        "POST / HTTP/1.0\r\n"
        "From: user@example.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/122.0.0.0 Safari/537.36\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Content-Type: multipart/form-data; "
        "boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Host: www.example.com\r\n"
        "\r\n";
    const std::string body =
        "This request includes headers and some body data (this text) that does not fit the input "
        "content buffer of 322 bytes.\r\n";

    auto result = fixture.parse_partially(headers + body);

    REQUIRE(result == RequestParser::good_part);
    std::vector<char> expectedContent = convertToCharVec("This request");
    REQUIRE(fixture.content_ == expectedContent);
    REQUIRE(fixture.request.getNoInitialBodyBytesReceived() == expectedContent.size());
    REQUIRE(fixture.request.body_ == expectedContent);
}

TEST_CASE("parse invalid request", "[request_parser]") {
    RequestFixture fixture(1024);
    SECTION("should return bad for invalid method") {
        const std::string request = "GE T /uri HTTP/1.1\r\n\r\n";

        auto result = fixture.parse_partially(request);

        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should return bad for invalid uri") {
        const std::string request = "GET /ur i HTTP/1.1\r\n\r\n";

        auto result = fixture.parse_partially(request);

        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should return bad for invalid http version") {
        const std::string request = "GET /uri HTTX/1.1\r\n\r\n";

        auto result = fixture.parse_partially(request);

        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should return bad for invalid header line") {
        const std::string request =
            "GET /uri HTTP/1.1\r\n"
            "Invalid-Header-Line\r\n"
            "\r\n";

        auto result = fixture.parse_partially(request);

        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should return bad for empty header name") {
        const std::string request =
            "GET /uri HTTP/1.1\r\n"
            ": no-name\r\n"
            "\r\n";

        auto result = fixture.parse_partially(request);

        REQUIRE(result == RequestParser::bad);
    }
}

TEST_CASE("Request parser 100-continue", "[request_parser]") {
    RequestFixture fixture(1024);

    SECTION("should detect Expect: 100-continue header") {
        const std::string request =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Length: 100\r\n"
            "Expect: 100-continue\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::good_headers_expect_continue);
        REQUIRE(fixture.request.expectsContinue() == true);
        REQUIRE(fixture.request.method_ == "POST");
        REQUIRE(fixture.request.uri_ == "/upload");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 1);
    }

    SECTION("should not trigger 100-continue for HTTP/1.0") {
        const std::string request =
            "POST /upload HTTP/1.0\r\n"
            "Host: example.com\r\n"
            "Content-Length: 100\r\n"
            "Expect: 100-continue\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

        // Should not return good_headers_expect_continue for HTTP/1.0
        REQUIRE(result != RequestParser::good_headers_expect_continue);
        REQUIRE(fixture.request.expectsContinue() == true);  // header is still parsed
    }

    SECTION("should handle case insensitive Expect header") {
        const std::string request =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Length: 100\r\n"
            "expect: 100-Continue\r\n"
            "\r\n";

        auto result = fixture.parse_complete(request);

        REQUIRE(result == RequestParser::good_headers_expect_continue);
        REQUIRE(fixture.request.expectsContinue() == true);
    }

    SECTION("should handle missing Content-Length with Expect: 100-continue") {
        const std::string request =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Expect: 100-continue\r\n"
            "\r\n";
        auto result = fixture.parse_complete(request);
        REQUIRE(result == RequestParser::missing_content_length);
    }
}
