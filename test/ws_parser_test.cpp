#include <catch2/catch_test_macros.hpp>
#include <string>
#include <iostream>

#include "ws_parser.hpp"
#include "ws_receiver.hpp"

using namespace beauty;

namespace {

}  // namespace

TEST_CASE("parse ws protocol", "[ws_parser]") {
	std::vector<char> maskedContent = {(char)0x81, (char)0x8c,(char)0x91, 0x3d, 0x43, 0x45, (char) 0xd9, 0x58, 0x2f, 0x29, (char) 0xfe, 0x1d, 0x14, 0x2a, (char) 0xe3, 0x51, 0x27, 0x64};
	WsReceiver wsRecv(maskedContent);
	WsParser dut(wsRecv);

    SECTION("should return indeterminate for empty content") {
		maskedContent.clear();
        REQUIRE(dut.parse() == WsParser::indeterminate);
    }
    SECTION("should parse masked content") {
        REQUIRE(dut.parse() == WsParser::done);
		std::string res(maskedContent.begin(), maskedContent.end());
		REQUIRE(res == "Hello World!");
		// for (int i = 0; i < maskedContent.size(); ++i) {
		// 	printf("%c", maskedContent[i]);
		// }
		// puts("");
    }
}

