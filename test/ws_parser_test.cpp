#include <catch2/catch_test_macros.hpp>
#include <string>
#include <iostream>

#include "beauty/ws_message.hpp"
#include "beauty/ws_parser.hpp"

using namespace beauty;

namespace {
// clang-format off
    const std::string contentShortLen =  "Hello World!";
    const std::vector<char> maskedContentShortLen = {(char)0x81, (char)0x8c,(char)0x91, 0x3d, 0x43, 0x45, (char) 0xd9, 0x58, 0x2f, 0x29, (char) 0xfe, 0x1d, 0x14, 0x2a, (char) 0xe3, 0x51, 0x27, 0x64};
    std::string alphapet = "abcdefghijklmnopqrstuvwxyz";
    std::string contentExtLen = alphapet + alphapet + alphapet + alphapet + alphapet;
    const std::vector<char> maskedContentExtLen = {(char)0x81, (char)0xfe, 0x00, (char)0x82, 0x68, 0x66, (char)0x9c, 0x74, 0x09, 0x04, (char)0xff, 0x10, 0x0d, 0x00, (char)0xfb, 0x1c, 0x01, 0x0c, (char)0xf7, 0x18, 0x05, 0x08, (char)0xf3, 0x04, 0x19, 0x14, (char)0xef, 0x00, 0x1d, 0x10, (char)0xeb, 0x0c, 0x11, 0x1c, (char)0xfd, 0x16, 0x0b, 0x02, (char)0xf9, 0x12, 0x0f, 0x0e, (char)0xf5, 0x1e, 0x03, 0x0a, (char)0xf1, 0x1a, 0x07, 0x16, (char)0xed, 0x06, 0x1b, 0x12, (char)0xe9, 0x02, 0x1f, 0x1e, (char)0xe5, 0x0e, 0x09, 0x04, (char)0xff, 0x10, 0x0d, 0x00, (char)0xfb, 0x1c, 0x01, 0x0c, (char)0xf7, 0x18, 0x05, 0x08, (char)0xf3, 0x04, 0x19, 0x14, (char)0xef, 0x00, 0x1d, 0x10, (char)0xeb, 0x0c, 0x11, 0x1c, (char)0xfd, 0x16, 0x0b, 0x02, (char)0xf9, 0x12, 0x0f, 0x0e, (char)0xf5, 0x1e, 0x03, 0x0a, (char)0xf1, 0x1a, 0x07, 0x16, (char)0xed, 0x06, 0x1b, 0x12, (char)0xe9, 0x02, 0x1f, 0x1e, (char)0xe5, 0x0e, 0x09, 0x04, (char)0xff, 0x10, 0x0d, 0x00, (char)0xfb, 0x1c, 0x01, 0x0c, (char)0xf7, 0x18, 0x05, 0x08, (char)0xf3, 0x04, 0x19, 0x14, (char)0xef, 0x00, 0x1d, 0x10, (char)0xeb, 0x0c, 0x11, 0x1c};
    
    // Close frame (opcode 8) with zero payload: FIN=1, opcode=8, masked, length=0
    std::vector<char> closeFrame = {(char)0x88, (char)0x80, (char)0xdc, (char)0xd9, 0x62, (char)0xfa};

    // Ping frame (opcode 9) with empty payload: FIN=1, opcode=9, masked, length=0
    const std::vector<char> pingFrameEmpty = {(char)0x89, (char)0x80, (char)0x12, (char)0x34, (char)0x56, (char)0x78};
    
    // Ping frame with "ping" payload: FIN=1, opcode=9, masked, length=4, payload="ping"
    const std::string pingPayload = "ping";
    const std::vector<char> pingFrameWithPayload = {(char)0x89, (char)0x84, (char)0x12, (char)0x34, (char)0x56, (char)0x78, 
                                                    (char)(0x70 ^ 0x12), (char)(0x69 ^ 0x34), (char)(0x6e ^ 0x56), (char)(0x67 ^ 0x78)};
    
    // Pong frame (opcode 10) with empty payload: FIN=1, opcode=10, masked, length=0
    const std::vector<char> pongFrameEmpty = {(char)0x8a, (char)0x80, (char)0xab, (char)0xcd, (char)0xef, (char)0x01};
    
        // Pong frame with "pong" payload: FIN=1, opcode=10, masked, length=4, payload="pong"  
    const std::string pongPayload = "pong";
    const std::vector<char> pongFrameWithPayload = {(char)0x8A, (char)0x84, (char)0x12, (char)0x34, (char)0x56, (char)0x78, 
                                                    (char)0x62, (char)0x5b, (char)0x38, (char)0x1f};

    // Fragmentation test frames (not supported)
    // Non-final text frame: FIN=0, opcode=1, masked, length=5, payload="hello"
    const std::vector<char> fragmentedTextStart = {(char)0x01, (char)0x85, (char)0x12, (char)0x34, (char)0x56, (char)0x78, 0x7A, 0x52, 0x22, 0x1E, 0x1B};
    // Continuation frame: FIN=1, opcode=0, masked, length=5, payload="world"  
    const std::vector<char> continuationFrame = {(char)0x80, (char)0x85, (char)0x12, (char)0x34, (char)0x56, (char)0x78, 0x65, 0x57, 0x32, 0x1A, 0x1B};
// clang-format on
// clang-format on

}  // namespace

TEST_CASE("parse ws protocol short len", "[ws_parser]") {
    std::vector<char> content = maskedContentShortLen;
    WsMessage wsMessage(content);
    WsParser dut(wsMessage);

    SECTION("should return indeterminate for empty content") {
        content.clear();
        REQUIRE(dut.parse() == WsParser::indeterminate);
    }
    SECTION("should parse masked content short len") {
        REQUIRE(dut.parse() == WsParser::data_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentShortLen);
        REQUIRE(wsMessage.isFinal_ == true);
    }
    SECTION("should parse multiple receive") {
        REQUIRE(dut.parse() == WsParser::data_frame);
        // restore content_ as parse() inplaces result
        wsMessage.content_ = maskedContentShortLen;
        wsMessage.reset();
        REQUIRE(dut.parse() == WsParser::data_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentShortLen);
        REQUIRE(wsMessage.isFinal_ == true);
    }
}

TEST_CASE("parse ws protocol ext len", "[ws_parser]") {
    std::vector<char> content = maskedContentExtLen;
    WsMessage wsMessage(content);
    WsParser dut(wsMessage);

    SECTION("should parse masked content ext len") {
        REQUIRE(dut.parse() == WsParser::data_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentExtLen);
        REQUIRE(wsMessage.isFinal_ == true);
    }
}

TEST_CASE("parse ws protocol in consecutive buffers", "[ws_parser]") {
    // max buffer size = 50 simulates that content spans several buffers
    std::vector<char> content(maskedContentExtLen.begin(), maskedContentExtLen.begin() + 50);
    WsMessage wsMessage(content);
    WsParser dut(wsMessage);

    SECTION("should parse masked content in consecutive buffers") {
        REQUIRE(dut.parse() == WsParser::indeterminate);
        std::string expectedContent = alphapet + alphapet.substr(0, 16);
        std::string res(content.begin(), content.end());
        REQUIRE(res == expectedContent);
        REQUIRE(wsMessage.isFinal_ == false);

        wsMessage.reset();
        wsMessage.content_.assign(maskedContentExtLen.begin() + 50,
                                  maskedContentExtLen.begin() + 100);
        REQUIRE(dut.parse() == WsParser::indeterminate);
        expectedContent = alphapet.substr(16) + alphapet + alphapet.substr(0, 14);
        res.assign(content.begin(), content.end());
        REQUIRE(res == expectedContent);
        REQUIRE(wsMessage.isFinal_ == false);

        wsMessage.reset();
        wsMessage.content_.assign(maskedContentExtLen.begin() + 100, maskedContentExtLen.end());
        REQUIRE(dut.parse() == WsParser::data_frame);
        expectedContent = alphapet.substr(14) + alphapet;
        res.assign(content.begin(), content.end());
        REQUIRE(res == expectedContent);
        REQUIRE(wsMessage.isFinal_ == true);
    }
}

TEST_CASE("parse op codes", "[ws_parser]") {
    std::vector<char> closeFrame = {
        (char)0x88, (char)0x80, (char)0xdc, (char)0xd9, 0x62, (char)0xfa};
    WsMessage wsMessage(closeFrame);
    WsParser dut(wsMessage);
    SECTION("should parse close") {
        REQUIRE(dut.parse() == WsParser::close_frame);
        std::string res(closeFrame.begin(), closeFrame.end());
        REQUIRE(res == "");  // Close frame with zero payload should result in empty string
        REQUIRE(wsMessage.isFinal_ == true);
    }
}

TEST_CASE("parse ping frames", "[ws_parser]") {
    SECTION("should parse empty ping frame") {
        std::vector<char> content = pingFrameEmpty;
        WsMessage wsMessage(content);
        WsParser dut(wsMessage);

        REQUIRE(dut.parse() == WsParser::ping_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == "");  // Empty ping frame should result in empty string
        REQUIRE(wsMessage.isFinal_ == true);
    }

    SECTION("should parse ping frame with payload") {
        std::vector<char> content = pingFrameWithPayload;
        WsMessage wsMessage(content);
        WsParser dut(wsMessage);

        REQUIRE(dut.parse() == WsParser::ping_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == pingPayload);  // Should unmask and return "ping"
        REQUIRE(wsMessage.isFinal_ == true);
    }
}

TEST_CASE("parse pong frames", "[ws_parser]") {
    SECTION("should parse empty pong frame") {
        std::vector<char> content = pongFrameEmpty;
        WsMessage wsMessage(content);
        WsParser dut(wsMessage);

        REQUIRE(dut.parse() == WsParser::pong_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == "");  // Empty pong frame should result in empty string
        REQUIRE(wsMessage.isFinal_ == true);
    }

    SECTION("should parse pong frame with payload") {
        std::vector<char> content = pongFrameWithPayload;
        WsMessage wsMessage(content);
        WsParser dut(wsMessage);

        REQUIRE(dut.parse() == WsParser::pong_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == pongPayload);  // Should unmask and return "pong"
        REQUIRE(wsMessage.isFinal_ == true);
    }
}

TEST_CASE("fragmentation rejection", "[ws_parser]") {
    SECTION("should reject non-final text frame") {
        std::vector<char> content = fragmentedTextStart;
        WsMessage wsMessage(content);
        WsParser dut(wsMessage);

        REQUIRE(dut.parse() == WsParser::fragmentation_error);
        // Parser should detect fragmentation early and reject
    }

    SECTION("should reject continuation frame") {
        std::vector<char> content = continuationFrame;
        WsMessage wsMessage(content);
        WsParser dut(wsMessage);

        REQUIRE(dut.parse() == WsParser::fragmentation_error);
        // Parser should reject continuation frames immediately
    }

    SECTION("should accept final text frame") {
        // Ensure we don't break normal text frames (FIN=1, opcode=1)
        std::vector<char> content = maskedContentShortLen;
        WsMessage wsMessage(content);
        WsParser dut(wsMessage);

        REQUIRE(dut.parse() == WsParser::data_frame);
        std::string res(content.begin(), content.end());
        REQUIRE(res == contentShortLen);
        REQUIRE(wsMessage.isFinal_ == true);
    }
}
