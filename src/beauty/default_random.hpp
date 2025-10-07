#pragma once
#include <random>
#include <cstdint>

#include "i_random_interface.hpp"

namespace beauty {

// Default implementation using standard library
class DefaultRandom : public IRandom {
   public:
    uint32_t generateRandom() override {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_int_distribution<uint32_t> dist;

        return dist(gen);
    }
};

}  // namespace beauty
