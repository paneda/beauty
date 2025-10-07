#pragma once

#include <cstdint>

namespace beauty {

// This interface allows platform-specific random implementations:
// - Embedded: Can use (if available) hardware accelerated random
// - General servers: Can use system random devices
class IRandom {
   public:
    virtual ~IRandom() = default;

    // return 32-bit random value
    virtual uint32_t generateRandom() = 0;
};

}  // namespace beauty