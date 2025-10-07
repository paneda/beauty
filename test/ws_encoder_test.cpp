#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>
#include <iostream>

#include "beauty/ws_encoder.hpp"
#include "utils/mock_random.hpp"

using namespace beauty;

namespace {
// clang-format off
    // Expected unmasked server frames (server->client frames are never masked)
    
    // Text frame: FIN=1, opcode=1, unmasked, length=12, payload="Hello World!"
    const std::string textContent = "Hello World!";
    const std::vector<char> expectedTextFrame = {(char)0x81, 0x0c, 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    
    // Binary frame: FIN=1, opcode=2, unmasked, length=4, payload=[0x01, 0x02, 0x03, 0x04]
    const std::vector<char> binaryContent = {0x01, 0x02, 0x03, 0x04};
    const std::vector<char> expectedBinaryFrame = {(char)0x82, 0x04, 0x01, 0x02, 0x03, 0x04};
    
    // Extended length text frame: FIN=1, opcode=1, unmasked, length=130 (requires 16-bit length)
    std::string longTextContent = "abcdefghijklmnopqrstuvwxyz" "abcdefghijklmnopqrstuvwxyz" "abcdefghijklmnopqrstuvwxyz" "abcdefghijklmnopqrstuvwxyz" "abcdefghijklmnopqrstuvwxyz"; // 130 chars
    const std::vector<char> expectedExtendedLengthFrame = {(char)0x81, 126, 0x00, (char)0x82}; // Header only, payload follows
    
    // Ping frame with empty payload: FIN=1, opcode=9, unmasked, length=0
    const std::vector<char> expectedEmptyPingFrame = {(char)0x89, 0x00};
    
    // Ping frame with "test" payload: FIN=1, opcode=9, unmasked, length=4, payload="test"
    const std::string pingContent = "test";
    const std::vector<char> expectedPingWithPayload = {(char)0x89, 0x04, 't', 'e', 's', 't'};
    
    // Pong frame with empty payload: FIN=1, opcode=10, unmasked, length=0
    const std::vector<char> expectedEmptyPongFrame = {(char)0x8A, 0x00};
    
    // Pong frame with "echo" payload: FIN=1, opcode=10, unmasked, length=4, payload="echo"
    const std::string pongContent = "echo";
    const std::vector<char> expectedPongWithPayload = {(char)0x8A, 0x04, 'e', 'c', 'h', 'o'};
    
    // Close frame with status 1000: FIN=1, opcode=8, unmasked, length=2, payload=[0x03, 0xe8]
    const std::vector<char> expectedCloseFrame1000 = {(char)0x88, 0x02, 0x03, (char)0xe8};
    
    // Close frame with status 1002 and reason "Protocol Error": FIN=1, opcode=8, unmasked
    const std::vector<char> expectedCloseFrameWithReason = {(char)0x88, 0x10, 0x03, (char)0xea, 'P', 'r', 'o', 't', 'o', 'c', 'o', 'l', ' ', 'E', 'r', 'r', 'o', 'r'};
// clang-format on
}  // namespace

TEST_CASE("encode text frames", "[ws_encoder]") {
    std::vector<char> buffer;
    WsEncoder encoder(buffer);

    SECTION("should encode short text frame") {
        encoder.encodeTextFrame(textContent);

        REQUIRE(buffer.size() == expectedTextFrame.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedTextFrame.begin()));
    }

    SECTION("should encode extended length text frame") {
        encoder.encodeTextFrame(longTextContent);

        // Check header (first 4 bytes)
        REQUIRE(buffer.size() == longTextContent.size() + 4);  // 4-byte header + payload
        REQUIRE(buffer[0] == expectedExtendedLengthFrame[0]);  // FIN + opcode
        REQUIRE(buffer[1] == expectedExtendedLengthFrame[1]);  // Length indicator (126)
        REQUIRE(buffer[2] == expectedExtendedLengthFrame[2]);  // Length high byte
        REQUIRE(buffer[3] == expectedExtendedLengthFrame[3]);  // Length low byte

        // Check payload
        std::string encodedPayload(buffer.begin() + 4, buffer.end());
        REQUIRE(encodedPayload == longTextContent);
    }

    SECTION("should encode non-final text frame") {
        encoder.encodeTextFrame(textContent, false);

        // First byte should have FIN=0 (0x01 instead of 0x81)
        REQUIRE(buffer[0] == 0x01);
        REQUIRE(buffer[1] == 0x0c);  // Length unchanged

        // Payload should be unchanged
        std::string encodedPayload(buffer.begin() + 2, buffer.end());
        REQUIRE(encodedPayload == textContent);
    }
}

TEST_CASE("encode binary frames", "[ws_encoder]") {
    std::vector<char> buffer;
    WsEncoder encoder(buffer);

    SECTION("should encode binary frame") {
        encoder.encodeBinaryFrame(binaryContent);

        REQUIRE(buffer.size() == expectedBinaryFrame.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedBinaryFrame.begin()));
    }

    SECTION("should encode empty binary frame") {
        std::vector<char> emptyContent;
        encoder.encodeBinaryFrame(emptyContent);

        std::vector<char> expectedEmpty = {(char)0x82, 0x00};
        REQUIRE(buffer.size() == expectedEmpty.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedEmpty.begin()));
    }
}

TEST_CASE("encode ping frames", "[ws_encoder]") {
    std::vector<char> buffer;
    WsEncoder encoder(buffer);

    SECTION("should encode empty ping frame") {
        encoder.encodePingFrame();

        REQUIRE(buffer.size() == expectedEmptyPingFrame.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedEmptyPingFrame.begin()));
    }

    SECTION("should encode ping frame with payload") {
        encoder.encodePingFrame(pingContent);

        REQUIRE(buffer.size() == expectedPingWithPayload.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedPingWithPayload.begin()));
    }

    SECTION("should encode ping frame with timestamp payload") {
        std::string timestamp = "1234567890";
        encoder.encodePingFrame(timestamp);

        // Check header
        REQUIRE(buffer[0] == (char)0x89);             // FIN=1, opcode=9
        REQUIRE(buffer[1] == (int)timestamp.size());  // Length

        // Check payload
        std::string encodedPayload(buffer.begin() + 2, buffer.end());
        REQUIRE(encodedPayload == timestamp);
    }
}

TEST_CASE("encode pong frames", "[ws_encoder]") {
    std::vector<char> buffer;
    WsEncoder encoder(buffer);

    SECTION("should encode empty pong frame with string") {
        encoder.encodePongFrame("");

        REQUIRE(buffer.size() == expectedEmptyPongFrame.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedEmptyPongFrame.begin()));
    }

    SECTION("should encode empty pong frame with vector") {
        std::vector<char> emptyPayload;
        encoder.encodePongFrame(emptyPayload);

        REQUIRE(buffer.size() == expectedEmptyPongFrame.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedEmptyPongFrame.begin()));
    }

    SECTION("should encode pong frame with string payload") {
        encoder.encodePongFrame(pongContent);

        REQUIRE(buffer.size() == expectedPongWithPayload.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedPongWithPayload.begin()));
    }

    SECTION("should encode pong frame with vector payload") {
        std::vector<char> vectorPayload(pongContent.begin(), pongContent.end());
        encoder.encodePongFrame(vectorPayload);

        REQUIRE(buffer.size() == expectedPongWithPayload.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedPongWithPayload.begin()));
    }

    SECTION("should echo ping payload in pong") {
        // Simulate receiving ping payload and echoing it back
        std::vector<char> pingPayload = {'p', 'i', 'n', 'g', '!', '!', '!'};
        encoder.encodePongFrame(pingPayload);

        // Check header
        REQUIRE(buffer[0] == (char)0x8A);               // FIN=1, opcode=10
        REQUIRE(buffer[1] == (int)pingPayload.size());  // Length

        // Check payload matches exactly
        std::vector<char> encodedPayload(buffer.begin() + 2, buffer.end());
        REQUIRE(std::equal(encodedPayload.begin(), encodedPayload.end(), pingPayload.begin()));
    }
}

TEST_CASE("encode close frames", "[ws_encoder]") {
    std::vector<char> buffer;
    WsEncoder encoder(buffer);

    SECTION("should encode close frame with default status 1000") {
        encoder.encodeCloseFrame();

        REQUIRE(buffer.size() == expectedCloseFrame1000.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedCloseFrame1000.begin()));
    }

    SECTION("should encode close frame with status 1000 explicitly") {
        encoder.encodeCloseFrame(1000);

        REQUIRE(buffer.size() == expectedCloseFrame1000.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedCloseFrame1000.begin()));
    }

    SECTION("should encode close frame with status and reason") {
        encoder.encodeCloseFrame(1002, "Protocol Error");

        REQUIRE(buffer.size() == expectedCloseFrameWithReason.size());
        REQUIRE(std::equal(buffer.begin(), buffer.end(), expectedCloseFrameWithReason.begin()));
    }

    SECTION("should encode close frame with various status codes") {
        // Test different standard close codes
        struct TestCase {
            uint16_t code;
            std::string reason;
            std::vector<char> expectedStart;
        };

        std::vector<TestCase> testCases = {
            {1001,
             "Going Away",
             {(char)0x88,
              0x0c,
              0x03,
              (char)0xe9,
              'G',
              'o',
              'i',
              'n',
              'g',
              ' ',
              'A',
              'w',
              'a',
              'y'}},
            {1003,
             "Invalid",
             {(char)0x88, 0x09, 0x03, (char)0xeb, 'I', 'n', 'v', 'a', 'l', 'i', 'd'}},
            {1008, "Policy", {(char)0x88, 0x08, 0x03, (char)0xf0, 'P', 'o', 'l', 'i', 'c', 'y'}}};

        for (const auto& testCase : testCases) {
            buffer.clear();
            encoder.encodeCloseFrame(testCase.code, testCase.reason);

            REQUIRE(buffer.size() == testCase.expectedStart.size());
            REQUIRE(std::equal(buffer.begin(), buffer.end(), testCase.expectedStart.begin()));
        }
    }
}

TEST_CASE("buffer reuse", "[ws_encoder]") {
    std::vector<char> buffer;
    WsEncoder encoder(buffer);

    SECTION("should clear buffer between encodings") {
        // Encode first frame
        encoder.encodePingFrame("first");
        size_t firstSize = buffer.size();

        // Encode second frame (should clear previous content)
        encoder.encodePongFrame("second");
        size_t secondSize = buffer.size();

        // Second frame should not contain first frame data
        REQUIRE(buffer[0] == (char)0x8A);  // Pong opcode, not ping
        REQUIRE(secondSize != firstSize);

        // Buffer should contain only pong frame
        std::string payload(buffer.begin() + 2, buffer.end());
        REQUIRE(payload == "second");
    }

    SECTION("should handle multiple encodings correctly") {
        // Test sequence of different frame types
        encoder.encodeTextFrame("hello");
        REQUIRE(buffer[0] == (char)0x81);

        encoder.encodeBinaryFrame({0x01, 0x02});
        REQUIRE(buffer[0] == (char)0x82);

        encoder.encodePingFrame("ping");
        REQUIRE(buffer[0] == (char)0x89);

        encoder.encodePongFrame("pong");
        REQUIRE(buffer[0] == (char)0x8A);

        encoder.encodeCloseFrame(1000);
        REQUIRE(buffer[0] == (char)0x88);
    }
}

TEST_CASE("frame size validation", "[ws_encoder]") {
    std::vector<char> buffer;
    WsEncoder encoder(buffer);

    SECTION("should handle different payload sizes correctly") {
        // Test 7-bit length (0-125 bytes)
        std::string small(125, 'x');
        encoder.encodeTextFrame(small);
        REQUIRE(buffer[1] == 125);  // Direct length encoding

        // Test 16-bit length (126-65535 bytes)
        std::string medium(200, 'y');
        encoder.encodeTextFrame(medium);
        REQUIRE(buffer[1] == 126);                        // Extended length indicator
        REQUIRE(buffer[2] == 0);                          // High byte of length
        REQUIRE(static_cast<uint8_t>(buffer[3]) == 200);  // Low byte of length

        // Test boundary case (exactly 126 bytes)
        std::string boundary(126, 'z');
        encoder.encodeTextFrame(boundary);
        REQUIRE(buffer[1] == 126);                        // Should use extended length
        REQUIRE(buffer[2] == 0);                          // High byte
        REQUIRE(static_cast<uint8_t>(buffer[3]) == 126);  // Low byte
    }
}

TEST_CASE("client frames are masked", "[ws_encoder]") {
    std::vector<char> buffer;
    MockRandom deterministicRng(0x12345678);
    WsEncoder encoder(buffer, deterministicRng);

    encoder.encodeTextFrame("test");

    // Should have MASK bit set in second byte
    REQUIRE((buffer[1] & 0x80) != 0);     // MASK bit set
                                          // Frame should be longer due to 4-byte mask key
    REQUIRE(buffer.size() == 2 + 4 + 4);  // header + mask + payload
}
