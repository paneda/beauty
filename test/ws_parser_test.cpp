#include <catch2/catch_test_macros.hpp>
#include <string>
#include <iostream>

#include "ws_parser.hpp"
#include "ws_receive.hpp"

using namespace beauty;

namespace {
// clang-format off
	const std::string contentShortLen =  "Hello World!";
	const std::vector<char> maskedContentShortLen = {(char)0x81, (char)0x8c,(char)0x91, 0x3d, 0x43, 0x45, (char) 0xd9, 0x58, 0x2f, 0x29, (char) 0xfe, 0x1d, 0x14, 0x2a, (char) 0xe3, 0x51, 0x27, 0x64};
	std::string alphapet = "abcdefghijklmnopqrstuvwxyz";
	std::string contentExtLen = alphapet + alphapet + alphapet + alphapet + alphapet;
	const std::vector<char> maskedContentExtLen = {(char)0x81, (char)0xfe, 0x00, (char)0x82, 0x68, 0x66, (char)0x9c, 0x74, 0x09, 0x04, (char)0xff, 0x10, 0x0d, 0x00, (char)0xfb, 0x1c, 0x01, 0x0c, (char)0xf7, 0x18, 0x05, 0x08, (char)0xf3, 0x04, 0x19, 0x14, (char)0xef, 0x00, 0x1d, 0x10, (char)0xeb, 0x0c, 0x11, 0x1c, (char)0xfd, 0x16, 0x0b, 0x02, (char)0xf9, 0x12, 0x0f, 0x0e, (char)0xf5, 0x1e, 0x03, 0x0a, (char)0xf1, 0x1a, 0x07, 0x16, (char)0xed, 0x06, 0x1b, 0x12, (char)0xe9, 0x02, 0x1f, 0x1e, (char)0xe5, 0x0e, 0x09, 0x04, (char)0xff, 0x10, 0x0d, 0x00, (char)0xfb, 0x1c, 0x01, 0x0c, (char)0xf7, 0x18, 0x05, 0x08, (char)0xf3, 0x04, 0x19, 0x14, (char)0xef, 0x00, 0x1d, 0x10, (char)0xeb, 0x0c, 0x11, 0x1c, (char)0xfd, 0x16, 0x0b, 0x02, (char)0xf9, 0x12, 0x0f, 0x0e, (char)0xf5, 0x1e, 0x03, 0x0a, (char)0xf1, 0x1a, 0x07, 0x16, (char)0xed, 0x06, 0x1b, 0x12, (char)0xe9, 0x02, 0x1f, 0x1e, (char)0xe5, 0x0e, 0x09, 0x04, (char)0xff, 0x10, 0x0d, 0x00, (char)0xfb, 0x1c, 0x01, 0x0c, (char)0xf7, 0x18, 0x05, 0x08, (char)0xf3, 0x04, 0x19, 0x14, (char)0xef, 0x00, 0x1d, 0x10, (char)0xeb, 0x0c, 0x11, 0x1c};
// clang-format on

}  // namespace

TEST_CASE("parse ws protocol short len", "[ws_parser]") {
    std::vector<char> content = maskedContentShortLen;
    WsReceive wsRecv(content);
    WsParser dut(wsRecv);

    SECTION("should return indeterminate for empty content") {
        content.clear();
        REQUIRE(dut.parse() == WsParser::indeterminate);
    }
    SECTION("should parse masked content short len") {
        REQUIRE(dut.parse() == WsParser::done);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentShortLen);
        REQUIRE(wsRecv.isFinal_ == true);
    }
    SECTION("should parse multiple receive") {
        REQUIRE(dut.parse() == WsParser::done);
        // restore content_ as parse() inplaces result
        wsRecv.content_ = maskedContentShortLen;
        wsRecv.reset();
        REQUIRE(dut.parse() == WsParser::done);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentShortLen);
        REQUIRE(wsRecv.isFinal_ == true);
    }
}

TEST_CASE("parse ws protocol ext len", "[ws_parser]") {
    std::vector<char> content = maskedContentExtLen;
    WsReceive wsRecv(content);
    WsParser dut(wsRecv);

    SECTION("should parse masked content ext len") {
        REQUIRE(dut.parse() == WsParser::done);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentExtLen);
        REQUIRE(wsRecv.isFinal_ == true);
    }
}

TEST_CASE("parse ws protocol in consecutive buffers", "[ws_parser]") {
    // max buffer size = 50 simulates that content spans several buffers
    std::vector<char> content(maskedContentExtLen.begin(), maskedContentExtLen.begin() + 50);
    WsReceive wsRecv(content);
    WsParser dut(wsRecv);

    SECTION("should parse masked content in consecutive buffers") {
        REQUIRE(dut.parse() == WsParser::indeterminate);
        std::string expectedContent = alphapet + alphapet.substr(0, 16);
        std::string res(content.begin(), content.end());
        REQUIRE(res == expectedContent);
        REQUIRE(wsRecv.isFinal_ == false);

        wsRecv.reset();
        wsRecv.content_.assign(maskedContentExtLen.begin() + 50, maskedContentExtLen.begin() + 100);
        REQUIRE(dut.parse() == WsParser::indeterminate);
        expectedContent = alphapet.substr(16) + alphapet + alphapet.substr(0, 14);
        res.assign(content.begin(), content.end());
        REQUIRE(res == expectedContent);
        REQUIRE(wsRecv.isFinal_ == false);

        wsRecv.reset();
        wsRecv.content_.assign(maskedContentExtLen.begin() + 100, maskedContentExtLen.end());
        REQUIRE(dut.parse() == WsParser::done);
        expectedContent = alphapet.substr(14) + alphapet;
        res.assign(content.begin(), content.end());
        REQUIRE(res == expectedContent);
        REQUIRE(wsRecv.isFinal_ == true);
    }
}

TEST_CASE("parse op codes", "[ws_parser]") {
    std::vector<char> content = {(char)0x88, (char)0x80, (char)0xdc, (char)0xd9, 0x62, (char)0xfa};
    WsReceive wsRecv(content);
    WsParser dut(wsRecv);
    SECTION("should parse close") {
        REQUIRE(dut.parse() == WsParser::done);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentExtLen);
        REQUIRE(wsRecv.isFinal_ == true);
    }
}
