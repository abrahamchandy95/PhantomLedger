#pragma once

#include <cstdint>

namespace PhantomLedger::random {

class Pcg64 {
public:
  Pcg64(std::uint64_t state_hi, std::uint64_t state_lo,
        std::uint64_t increment_hi, std::uint64_t increment_lo) noexcept
      : state_hi_(state_hi), state_lo_(state_lo), increment_hi_(increment_hi),
        increment_lo_(increment_lo) {}

  [[nodiscard]] std::uint64_t next_u64() noexcept {
    step();
    return output();
  }

  [[nodiscard]] double next_double() noexcept {
    return static_cast<double>(next_u64() >> 11U) * 0x1.0p-53;
  }

  [[nodiscard]] std::uint64_t state_hi() const noexcept { return state_hi_; }
  [[nodiscard]] std::uint64_t state_lo() const noexcept { return state_lo_; }
  [[nodiscard]] std::uint64_t increment_hi() const noexcept {
    return increment_hi_;
  }
  [[nodiscard]] std::uint64_t increment_lo() const noexcept {
    return increment_lo_;
  }

private:
  static constexpr std::uint64_t multiplier_hi_ = 2549297995355413924ULL;
  static constexpr std::uint64_t multiplier_lo_ = 4865540595714422341ULL;

  std::uint64_t state_hi_ = 0;
  std::uint64_t state_lo_ = 0;
  std::uint64_t increment_hi_ = 0;
  std::uint64_t increment_lo_ = 0;

  static void multiply_mod_2_128(std::uint64_t lhs_hi, std::uint64_t lhs_lo,
                                 std::uint64_t rhs_hi, std::uint64_t rhs_lo,
                                 std::uint64_t &out_hi,
                                 std::uint64_t &out_lo) noexcept {
#if defined(__SIZEOF_INT128__)
    const __uint128_t lhs = (static_cast<__uint128_t>(lhs_hi) << 64U) | lhs_lo;
    const __uint128_t rhs = (static_cast<__uint128_t>(rhs_hi) << 64U) | rhs_lo;
    const __uint128_t product = lhs * rhs;
    out_hi = static_cast<std::uint64_t>(product >> 64U);
    out_lo = static_cast<std::uint64_t>(product);
#else
    // Portable fallback. We only need the low 128 bits because the LCG is
    // defined modulo 2^128.
    const std::uint64_t lhs_lo_lo = lhs_lo & 0xFFFFFFFFULL;
    const std::uint64_t lhs_lo_hi = lhs_lo >> 32U;
    const std::uint64_t rhs_lo_lo = rhs_lo & 0xFFFFFFFFULL;
    const std::uint64_t rhs_lo_hi = rhs_lo >> 32U;

    const std::uint64_t p0 = lhs_lo_lo * rhs_lo_lo;
    const std::uint64_t p1 = lhs_lo_lo * rhs_lo_hi;
    const std::uint64_t p2 = lhs_lo_hi * rhs_lo_lo;
    const std::uint64_t p3 = lhs_lo_hi * rhs_lo_hi;

    const std::uint64_t middle =
        (p0 >> 32U) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL);
    out_lo = (p0 & 0xFFFFFFFFULL) | (middle << 32U);
    out_hi = p3 + (p1 >> 32U) + (p2 >> 32U) + (middle >> 32U);
    out_hi += lhs_hi * rhs_lo + lhs_lo * rhs_hi;
#endif
  }

  static void add_mod_2_128(std::uint64_t lhs_hi, std::uint64_t lhs_lo,
                            std::uint64_t rhs_hi, std::uint64_t rhs_lo,
                            std::uint64_t &out_hi,
                            std::uint64_t &out_lo) noexcept {
    out_lo = lhs_lo + rhs_lo;
    out_hi = lhs_hi + rhs_hi + (out_lo < lhs_lo ? 1ULL : 0ULL);
  }

  void step() noexcept {
    std::uint64_t product_hi = 0;
    std::uint64_t product_lo = 0;
    multiply_mod_2_128(state_hi_, state_lo_, multiplier_hi_, multiplier_lo_,
                       product_hi, product_lo);
    add_mod_2_128(product_hi, product_lo, increment_hi_, increment_lo_,
                  state_hi_, state_lo_);
  }

  [[nodiscard]] std::uint64_t output() const noexcept {
    const std::uint64_t xor_value = state_hi_ ^ state_lo_;
    const unsigned rotation = static_cast<unsigned>(state_hi_ >> 58U);
    return (xor_value >> rotation) | (xor_value << ((-rotation) & 63U));
  }
};

} // namespace PhantomLedger::random
