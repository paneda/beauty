#pragma once

#include "beauty/i_random_interface.hpp"

namespace beauty {
class MockRandom : public IRandom {
   public:
    MockRandom(uint32_t fixedValue = 0xDEADBEEF) : value_(fixedValue) {}

    // Deterministic "random" for testing
    uint32_t generateRandom() override {
        return value_++;  // Increment for variety in tests
    }

   private:
    uint32_t value_;
};

}  // namespace beauty
