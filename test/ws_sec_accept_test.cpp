#include <catch2/catch_test_macros.hpp>

#include "ws_sec_accept.hpp"

using namespace beauty;

TEST_CASE("compute", "[ws_sec_accept]") {
    SECTION("it should compute example from RFC6455") {
        const char* key = " dGhlIHNhbXBsZSBub25jZQ== ";
        WsSecAccept dut;
        auto res = dut.compute(key);
        REQUIRE(res == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }
}
