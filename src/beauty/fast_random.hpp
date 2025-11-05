#pragma once

#include "i_random_interface.hpp"

namespace beauty {

// May be used in embedded random implementations. Uses a simple PRNG
// suitable for non-cryptographic purposes. Much faster than
// std::random_device.
class FastRandom : public IRandom {
   public:
    FastRandom(uint32_t seed = 0x12345678) : state_(seed) {
        if (state_ == 0) {
            state_ = 0x12345678;  // Ensure non-zero state
        }
    }

    uint32_t generateRandom() override {
        // xorshift32 PRNG - very fast but not cryptographically secure
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

   private:
    uint32_t state_;
};

}  // namespace beauty
