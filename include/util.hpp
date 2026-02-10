#pragma once
#include <cstdint>
#include <cstddef>
#include <stdexcept>

inline bool is_pow2(std::size_t x) { return x && ((x & (x - 1)) == 0); }

inline std::size_t ilog2_pow2(std::size_t x) {
    if (!is_pow2(x)) throw std::invalid_argument("ilog2_pow2 requires power-of-two");
    std::size_t r = 0;
    while (x > 1) { x >>= 1; ++r; }
    return r;
}
