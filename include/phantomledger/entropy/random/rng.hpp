#pragma once

#include "pcg64.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::random {

class Rng {
public:
  explicit Rng(Pcg64 engine) noexcept : engine_(engine) {}

  /// Construct from a 64-bit seed via SplitMix64 expansion.
  [[nodiscard]] static Rng fromSeed(std::uint64_t seed) {
    std::uint64_t state = seed;
    const std::uint64_t s0 = splitmix64(state);
    const std::uint64_t s1 = splitmix64(state);
    const std::uint64_t i0 = splitmix64(state);
    const std::uint64_t i1 = splitmix64(state) | 1ULL;
    return Rng(Pcg64(s0, s1, i0, i1));
  }

  [[nodiscard]] Pcg64 &engine() noexcept { return engine_; }
  [[nodiscard]] const Pcg64 &engine() const noexcept { return engine_; }

  // ----- Scalar samplers -----

  [[nodiscard]] bool coin(double p) noexcept {
    if (p <= 0.0) {
      return false;
    }
    if (p >= 1.0) {
      return true;
    }
    return nextDouble() < p;
  }

  [[nodiscard]] std::int64_t uniformInt(std::int64_t low, std::int64_t high) {
    if (low >= high) {
      throw std::invalid_argument("uniformInt: low must be < high");
    }

    const auto range = static_cast<std::uint64_t>(high - low);
    return low + static_cast<std::int64_t>(bounded(range));
  }

  [[nodiscard]] double nextDouble() noexcept { return engine_.next_double(); }
  [[nodiscard]] std::uint64_t nextU64() noexcept { return engine_.next_u64(); }

  // ----- Collection samplers -----

  /// Pick one index uniformly from [0, n).
  [[nodiscard]] std::size_t choiceIndex(std::size_t n) {
    if (n == 0) {
      throw std::invalid_argument("choiceIndex: n must be > 0");
    }

    return static_cast<std::size_t>(bounded(static_cast<std::uint64_t>(n)));
  }

  /// Pick k indices from [0, n). Without replacement by default.
  [[nodiscard]] std::vector<std::size_t>
  choiceIndices(std::size_t n, std::size_t k, bool replace = false) {
    if (k == 0) {
      return {};
    }
    if (n == 0) {
      throw std::invalid_argument("choiceIndices: n must be > 0 when k > 0");
    }
    if (!replace && k > n) {
      throw std::invalid_argument(
          "choiceIndices: k exceeds n without replacement");
    }

    if (replace) {
      std::vector<std::size_t> out(k);
      for (auto &idx : out) {
        idx = static_cast<std::size_t>(bounded(static_cast<std::uint64_t>(n)));
      }
      return out;
    }

    // Partial Fisher-Yates shuffle.
    std::vector<std::size_t> pool(n);
    std::iota(pool.begin(), pool.end(), std::size_t{0});

    for (std::size_t i = 0; i < k; ++i) {
      const auto j = i + static_cast<std::size_t>(
                             bounded(static_cast<std::uint64_t>(n - i)));
      std::swap(pool[i], pool[j]);
    }

    pool.resize(k);
    return pool;
  }

private:
  static constexpr std::uint64_t splitmixGamma_ = 0x9e3779b97f4a7c15ULL;
  static constexpr std::uint64_t splitmixMix1_ = 0xbf58476d1ce4e5b9ULL;
  static constexpr std::uint64_t splitmixMix2_ = 0x94d049bb133111ebULL;

  [[nodiscard]] static std::uint64_t splitmix64(std::uint64_t &state) noexcept {
    state += splitmixGamma_;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * splitmixMix1_;
    value = (value ^ (value >> 27U)) * splitmixMix2_;
    return value ^ (value >> 31U);
  }

  /// Lemire's nearly-divisionless bounded random.
  [[nodiscard]] std::uint64_t bounded(std::uint64_t range) noexcept {
    if (range <= 1) {
      return 0;
    }

    std::uint64_t x = engine_.next_u64();

#if defined(__SIZEOF_INT128__)
    __uint128_t product =
        static_cast<__uint128_t>(x) * static_cast<__uint128_t>(range);
    auto low = static_cast<std::uint64_t>(product);

    if (low < range) {
      const std::uint64_t threshold =
          static_cast<std::uint64_t>(-range) % range;
      while (low < threshold) {
        x = engine_.next_u64();
        product = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(range);
        low = static_cast<std::uint64_t>(product);
      }
    }

    return static_cast<std::uint64_t>(product >> 64U);
#else
    const std::uint64_t limit =
        (std::numeric_limits<std::uint64_t>::max() / range) * range;

    while (x >= limit) {
      x = engine_.next_u64();
    }

    return x % range;
#endif
  }

  Pcg64 engine_;
};

} // namespace PhantomLedger::random
