#include <catch2/catch_test_macros.hpp>

#include "beauty/i_random_interface.hpp"
#include "beauty/default_random.hpp"
#include "beauty/fast_random.hpp"
#include "utils/mock_random.hpp"
#include "beauty/ws_encoder.hpp"

using namespace beauty;

TEST_CASE("IRandom interface implementations", "[random_interface]") {
    SECTION("DefaultRandom should generate different values") {
        DefaultRandom rng;

        uint32_t value1 = rng.generateRandom();
        uint32_t value2 = rng.generateRandom();
        uint32_t value3 = rng.generateRandom();

        // Very unlikely to get same value three times
        REQUIRE_FALSE((value1 == value2 && value2 == value3));
    }

    SECTION("FastRandom should generate deterministic sequence") {
        FastRandom rng1(12345);
        FastRandom rng2(12345);  // Same seed

        // Same seed should produce same sequence
        REQUIRE(rng1.generateRandom() == rng2.generateRandom());
        REQUIRE(rng1.generateRandom() == rng2.generateRandom());
        REQUIRE(rng1.generateRandom() == rng2.generateRandom());
    }

    SECTION("FastRandom should generate different values from different seeds") {
        FastRandom rng1(12345);
        FastRandom rng2(54321);

        // Different seeds should produce different values
        REQUIRE(rng1.generateRandom() != rng2.generateRandom());
    }

    SECTION("FastRandom should never return zero") {
        FastRandom rng(1);  // Start with small seed

        // Generate many values to ensure none are zero
        for (int i = 0; i < 1000; ++i) {
            REQUIRE(rng.generateRandom() != 0);
        }
    }

    SECTION("MockRandom should increment") {
        MockRandom rng(0x1000);

        REQUIRE(rng.generateRandom() == 0x1000);
        REQUIRE(rng.generateRandom() == 0x1001);
        REQUIRE(rng.generateRandom() == 0x1002);
    }
}

TEST_CASE("WsEncoder with custom random", "[ws_encoder][random]") {
    std::vector<char> buffer;
    MockRandom deterministicRng(0x12345678);

    SECTION("should use custom random for client masking") {
        WsEncoder encoder(buffer, deterministicRng);
        encoder.encodeTextFrame("test");

        // Verify mask key bytes match our deterministic value
        // Mask starts at buffer[2] (after opcode and length)
        REQUIRE(static_cast<uint8_t>(buffer[2]) == 0x78);  // Low byte
        REQUIRE(static_cast<uint8_t>(buffer[3]) == 0x56);  // Next byte
        REQUIRE(static_cast<uint8_t>(buffer[4]) == 0x34);  // Next byte
        REQUIRE(static_cast<uint8_t>(buffer[5]) == 0x12);  // High byte
    }

    SECTION("should not use random for server frames") {
        WsEncoder encoder(buffer);
        encoder.encodeTextFrame("test");

        // Server frames should not be masked regardless of random provider
        REQUIRE((buffer[1] & 0x80) == 0);  // MASK bit not set
        REQUIRE(buffer.size() == 2 + 4);   // header + payload (no mask)
    }
}

TEST_CASE("Performance comparison", "[random_interface][performance]") {
    const int iterations = 10000;

    SECTION("FastRandom should be faster than DefaultRandom") {
        // This is more of a smoke test - actual performance would need benchmarking
        FastRandom fastRng;
        DefaultRandom defaultRng;

        // Just verify both work for many iterations
        for (int i = 0; i < iterations; ++i) {
            uint32_t fast = fastRng.generateRandom();
            uint32_t def = defaultRng.generateRandom();

            // Both should produce non-zero values
            REQUIRE(fast != 0);
            REQUIRE(def != 0);
        }
    }
}