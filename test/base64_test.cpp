#include <catch2/catch_test_macros.hpp>

#include <vector>
#include "beauty/base64.hpp"

using namespace beauty;

TEST_CASE("encode", "[base64]") {
    SECTION("it should encode example from RFC6455") {
        std::vector<unsigned char> data = {0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f,
                                           0x16, 0x90, 0xf6, 0x46, 0x06, 0xcf, 0x38,
                                           0x59, 0x45, 0xb2, 0xbe, 0xc4, 0xea};
        auto res = base64_encode(&data[0], data.size());
        REQUIRE(res == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }
}
