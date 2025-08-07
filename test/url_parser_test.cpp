#include <catch2/catch_test_macros.hpp>

#include "url_parser.hpp"

using namespace beauty;

TEST_CASE("http", "[url_parser]") {
    SECTION("it should parse the url") {
        const char *text =
            "http://www.example.com/dir/subdir?param=1&param=2;param%20=%20#fragment";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "http");
        REQUIRE(parser.hostname() == "www.example.com");
        REQUIRE(parser.path() == "/dir/subdir");
        REQUIRE(parser.query() == "param=1&param=2;param%20=%20");
        REQUIRE(parser.fragment() == "fragment");
        REQUIRE(parser.httpPort() == 80);
    }
    SECTION("it should parse only hostname") {
        const char *text = "http://www.example.com";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "http");
        REQUIRE(parser.hostname() == "www.example.com");
        REQUIRE(parser.path() == "/");
    }

    SECTION("it should parse url with username") {
        const char *text =
            "http://username@www.example.com/dir/subdir?param=1&param=2;param%20=%20#fragment";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "http");
        REQUIRE(parser.username() == "username");
        REQUIRE(parser.hostname() == "www.example.com");
        REQUIRE(parser.path() == "/dir/subdir");
        REQUIRE(parser.query() == "param=1&param=2;param%20=%20");
        REQUIRE(parser.fragment() == "fragment");
        REQUIRE(parser.httpPort() == 80);
    }

    SECTION("it should parse url with username and password") {
        const char *text =
            "http://username:passwd@www.example.com/dir/"
            "subdir?param=1&param=2;param%20=%20#fragment";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "http");
        REQUIRE(parser.username() == "username");
        REQUIRE(parser.password() == "passwd");
        REQUIRE(parser.hostname() == "www.example.com");
        REQUIRE(parser.path() == "/dir/subdir");
        REQUIRE(parser.query() == "param=1&param=2;param%20=%20");
        REQUIRE(parser.fragment() == "fragment");
        REQUIRE(parser.httpPort() == 80);
    }

    SECTION("it should parse url with port") {
        const char *text =
            "http://www.example.com:8080/dir/subdir?param=1&param=2;param%20=%20#fragment";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "http");
        REQUIRE(parser.hostname() == "www.example.com");
        REQUIRE(parser.path() == "/dir/subdir");
        REQUIRE(parser.query() == "param=1&param=2;param%20=%20");
        REQUIRE(parser.fragment() == "fragment");
        REQUIRE(parser.httpPort() == 8080);
    }
    SECTION("it should parse url with port") {
        const char *text =
            "http://username:passwd@www.example.com:8080/dir/"
            "subdir?param=1&param=2;param%20=%20#fragment";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "http");
        REQUIRE(parser.username() == "username");
        REQUIRE(parser.password() == "passwd");
        REQUIRE(parser.hostname() == "www.example.com");
        REQUIRE(parser.path() == "/dir/subdir");
        REQUIRE(parser.query() == "param=1&param=2;param%20=%20");
        REQUIRE(parser.fragment() == "fragment");
        REQUIRE(parser.httpPort() == 8080);
    }

    SECTION("it should parse ftp") {
        const char *text = "ftp://username:passwd@ftp.example.com/dir/filename.ext";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "ftp");
        REQUIRE(parser.username() == "username");
        REQUIRE(parser.password() == "passwd");
        REQUIRE(parser.hostname() == "ftp.example.com");
        REQUIRE(parser.path() == "/dir/filename.ext");
        REQUIRE(parser.query() == "");
        REQUIRE(parser.fragment() == "");
    }

    SECTION("it should parse email url") {
        const char *text = "mailto:username@example.com";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "mailto");
        REQUIRE(parser.username() == "username");
        REQUIRE(parser.hostname() == "example.com");
    }

    SECTION("it should parse git over ssh") {
        const char *text = "git+ssh://hostname-01.org/path/to/file";
        UrlParser parser(text);

        REQUIRE(parser.isValid() == true);
        REQUIRE(parser.scheme() == "git+ssh");
        REQUIRE(parser.hostname() == "hostname-01.org");
        REQUIRE(parser.path() == "/path/to/file");
    }
}
