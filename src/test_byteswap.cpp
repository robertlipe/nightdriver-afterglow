#include <bit>
#include <cstdint>
#include <numeric>

uint32_t test_bs(uint32_t x) { return std::byteswap(x); }
uint8_t test_add_sat(uint8_t a, uint8_t b) { return std::add_sat(a, b); }
uint8_t test_sub_sat(uint8_t a, uint8_t b) { return std::sub_sat(a, b); }
