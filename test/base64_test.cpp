#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <cstring>
#include "beauty/base64.hpp"

using namespace beauty;

TEST_CASE("base64_encode", "[base64]") {
    SECTION("it should encode example from RFC6455") {
        std::vector<unsigned char> data = {0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f,
                                           0x16, 0x90, 0xf6, 0x46, 0x06, 0xcf, 0x38,
                                           0x59, 0x45, 0xb2, 0xbe, 0xc4, 0xea};
        auto res = base64_encode(&data[0], data.size());
        REQUIRE(res == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    SECTION("it should encode empty data") {
        std::vector<unsigned char> data = {};
        auto res = base64_encode(data.data(), data.size());
        REQUIRE(res == "");
    }

    SECTION("it should encode single byte") {
        std::vector<unsigned char> data = {0x41};  // 'A'
        auto res = base64_encode(&data[0], data.size());
        REQUIRE(res == "QQ==");
    }

    SECTION("it should encode two bytes") {
        std::vector<unsigned char> data = {0x41, 0x42};  // 'AB'
        auto res = base64_encode(&data[0], data.size());
        REQUIRE(res == "QUI=");
    }

    SECTION("it should encode three bytes") {
        std::vector<unsigned char> data = {0x41, 0x42, 0x43};  // 'ABC'
        auto res = base64_encode(&data[0], data.size());
        REQUIRE(res == "QUJD");
    }

    SECTION("it should encode simple text") {
        const char* text = "Hello, World!";
        auto res = base64_encode(reinterpret_cast<const unsigned char*>(text), strlen(text));
        REQUIRE(res == "SGVsbG8sIFdvcmxkIQ==");
    }
}

TEST_CASE("base64_decode", "[base64]") {
    SECTION("it should decode example from RFC6455") {
        char const* encoded = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
        std::string decoded = base64_decode(encoded, strlen(encoded));
        std::vector<unsigned char> expected = {0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f,
                                               0x16, 0x90, 0xf6, 0x46, 0x06, 0xcf, 0x38,
                                               0x59, 0x45, 0xb2, 0xbe, 0xc4, 0xea};
        REQUIRE(decoded.size() == expected.size());
        REQUIRE(std::equal(
            decoded.begin(), decoded.end(), expected.begin(), [](char a, unsigned char b) {
                return static_cast<unsigned char>(a) == b;
            }));
    }

    SECTION("it should decode empty string") {
        std::string decoded = base64_decode("", 0);
        REQUIRE(decoded.empty());
    }

    SECTION("it should decode single byte with padding") {
        std::string decoded = base64_decode("QQ==", 4);
        REQUIRE(decoded.size() == 1);
        REQUIRE(static_cast<unsigned char>(decoded[0]) == 0x41);
    }

    SECTION("it should decode two bytes with padding") {
        std::string decoded = base64_decode("QUI=", 4);
        REQUIRE(decoded.size() == 2);
        REQUIRE(static_cast<unsigned char>(decoded[0]) == 0x41);
        REQUIRE(static_cast<unsigned char>(decoded[1]) == 0x42);
    }

    SECTION("it should decode three bytes without padding") {
        std::string decoded = base64_decode("QUJD", 4);
        REQUIRE(decoded.size() == 3);
        REQUIRE(static_cast<unsigned char>(decoded[0]) == 0x41);
        REQUIRE(static_cast<unsigned char>(decoded[1]) == 0x42);
        REQUIRE(static_cast<unsigned char>(decoded[2]) == 0x43);
    }

    SECTION("it should decode simple text") {
        std::string decoded = base64_decode("SGVsbG8sIFdvcmxkIQ==", 20);
        REQUIRE(decoded == "Hello, World!");
    }
}

TEST_CASE("base64_encode_to_buffer", "[base64]") {
    SECTION("it should encode example from RFC6455 to buffer") {
        std::vector<unsigned char> data = {0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f,
                                           0x16, 0x90, 0xf6, 0x46, 0x06, 0xcf, 0x38,
                                           0x59, 0x45, 0xb2, 0xbe, 0xc4, 0xea};
        char buffer[100];
        size_t written = base64_encode_to_buffer(data.data(), data.size(), buffer, sizeof(buffer));
        REQUIRE(written == 28);
        REQUIRE(std::string(buffer) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    SECTION("it should encode empty data to buffer") {
        char buffer[10];
        size_t written = base64_encode_to_buffer(nullptr, 0, buffer, sizeof(buffer));
        REQUIRE(written == 0);
        REQUIRE(buffer[0] == '\0');
    }

    SECTION("it should encode single byte to buffer") {
        unsigned char data[] = {0x41};
        char buffer[10];
        size_t written = base64_encode_to_buffer(data, 1, buffer, sizeof(buffer));
        REQUIRE(written == 4);
        REQUIRE(std::string(buffer) == "QQ==");
    }

    SECTION("it should encode simple text to buffer") {
        const char* text = "Hello, World!";
        char buffer[100];
        size_t written = base64_encode_to_buffer(
            reinterpret_cast<const unsigned char*>(text), strlen(text), buffer, sizeof(buffer));
        REQUIRE(written == 20);
        REQUIRE(std::string(buffer) == "SGVsbG8sIFdvcmxkIQ==");
    }

    SECTION("it should fail with insufficient buffer size") {
        unsigned char data[] = {0x41, 0x42, 0x43};
        char buffer[5];  // Need at least 5 bytes (4 + null terminator)
        size_t written = base64_encode_to_buffer(data, 3, buffer, sizeof(buffer));
        REQUIRE(written == 4);  // Should succeed with exactly 5 bytes

        char small_buffer[4];  // Too small
        written = base64_encode_to_buffer(data, 3, small_buffer, sizeof(small_buffer));
        REQUIRE(written == 0);  // Should fail
    }
}

TEST_CASE("base64_decode_to_buffer", "[base64]") {
    SECTION("it should decode example from RFC6455 to buffer") {
        char const* encoded = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
        unsigned char buffer[100];
        size_t written = base64_decode_to_buffer(encoded, strlen(encoded), buffer, sizeof(buffer));

        std::vector<unsigned char> expected = {0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f,
                                               0x16, 0x90, 0xf6, 0x46, 0x06, 0xcf, 0x38,
                                               0x59, 0x45, 0xb2, 0xbe, 0xc4, 0xea};
        REQUIRE(written == expected.size());
        REQUIRE(std::equal(buffer, buffer + written, expected.begin()));
    }

    SECTION("it should decode empty string") {
        unsigned char buffer[10];
        size_t written = base64_decode_to_buffer("", 0, buffer, sizeof(buffer));
        REQUIRE(written == 0);
    }

    SECTION("it should decode single byte with padding to buffer") {
        unsigned char buffer[10];
        size_t written = base64_decode_to_buffer("QQ==", 4, buffer, sizeof(buffer));
        REQUIRE(written == 1);
        REQUIRE(buffer[0] == 0x41);
    }

    SECTION("it should decode two bytes with padding to buffer") {
        unsigned char buffer[10];
        size_t written = base64_decode_to_buffer("QUI=", 4, buffer, sizeof(buffer));
        REQUIRE(written == 2);
        REQUIRE(buffer[0] == 0x41);
        REQUIRE(buffer[1] == 0x42);
    }

    SECTION("it should decode three bytes without padding to buffer") {
        unsigned char buffer[10];
        size_t written = base64_decode_to_buffer("QUJD", 4, buffer, sizeof(buffer));
        REQUIRE(written == 3);
        REQUIRE(buffer[0] == 0x41);
        REQUIRE(buffer[1] == 0x42);
        REQUIRE(buffer[2] == 0x43);
    }

    SECTION("it should decode simple text to buffer") {
        unsigned char buffer[100];
        size_t written =
            base64_decode_to_buffer("SGVsbG8sIFdvcmxkIQ==", 20, buffer, sizeof(buffer));
        REQUIRE(written == 13);
        REQUIRE(std::string(reinterpret_cast<char*>(buffer), written) == "Hello, World!");
    }

    SECTION("it should fail with insufficient buffer size") {
        unsigned char buffer[2];  // Too small for 3 decoded bytes
        size_t written = base64_decode_to_buffer("QUJD", 4, buffer, sizeof(buffer));
        REQUIRE(written == 0);  // Should fail
    }

    SECTION("it should handle non-padded base64 input") {
        // "QUJ" is valid non-padded base64 for "AB"
        unsigned char buffer[10];
        size_t written = base64_decode_to_buffer("QUJ", 3, buffer, sizeof(buffer));
        REQUIRE(written == 2);       // Should decode successfully
        REQUIRE(buffer[0] == 0x41);  // 'A'
        REQUIRE(buffer[1] == 0x42);  // 'B'
    }

    SECTION("it should handle non-padded base64 input") {
        // Non-padded base64 (missing padding characters)
        unsigned char buffer[10];
        size_t written = base64_decode_to_buffer("QUJDREVGRw", 10, buffer, sizeof(buffer));
        REQUIRE(written > 0);        // Should succeed with non-padded input
        REQUIRE(buffer[0] == 0x41);  // 'A'
        REQUIRE(buffer[1] == 0x42);  // 'B'
    }

    SECTION("it should reject invalid base64 characters") {
        unsigned char buffer[10];
        // '@' is not a valid base64 character
        size_t written = base64_decode_to_buffer("QUJ@", 4, buffer, sizeof(buffer));
        REQUIRE(written == 0);  // Should fail
    }

    SECTION("it should reject whitespace in input") {
        unsigned char buffer[10];
        size_t written = base64_decode_to_buffer("QU JD", 5, buffer, sizeof(buffer));
        REQUIRE(written == 0);  // Should fail - space is not valid
    }
}

TEST_CASE("base64_roundtrip", "[base64]") {
    SECTION("encode then decode should produce original data") {
        std::vector<unsigned char> original = {0x00,
                                               0x01,
                                               0x02,
                                               0x03,
                                               0xFF,
                                               0xFE,
                                               0xFD,
                                               0xFC,
                                               0x80,
                                               0x7F,
                                               0x40,
                                               0x3F,
                                               0x20,
                                               0x1F,
                                               0x10,
                                               0x0F};

        // Encode
        std::string encoded = base64_encode(original.data(), original.size());

        // Decode
        std::string decoded = base64_decode(encoded.c_str(), encoded.size());

        // Verify
        REQUIRE(decoded.size() == original.size());
        REQUIRE(std::equal(
            decoded.begin(), decoded.end(), original.begin(), [](char a, unsigned char b) {
                return static_cast<unsigned char>(a) == b;
            }));
    }

    SECTION("buffer versions roundtrip should produce original data") {
        std::vector<unsigned char> original = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};

        // Encode to buffer
        char encode_buffer[100];
        size_t encoded_len = base64_encode_to_buffer(
            original.data(), original.size(), encode_buffer, sizeof(encode_buffer));
        REQUIRE(encoded_len > 0);

        // Decode to buffer
        unsigned char decode_buffer[100];
        size_t decoded_len = base64_decode_to_buffer(
            encode_buffer, encoded_len, decode_buffer, sizeof(decode_buffer));

        // Verify
        REQUIRE(decoded_len == original.size());
        REQUIRE(std::equal(decode_buffer, decode_buffer + decoded_len, original.begin()));
    }
}
