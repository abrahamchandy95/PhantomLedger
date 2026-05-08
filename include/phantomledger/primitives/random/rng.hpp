#pragma once

#include "pcg64.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::random {

class Rng {
public:
  explicit Rng(Pcg64 engine) noexcept : engine_(engine) {}

  /// Construct from a 64-bit seed via SplitMix64 expansion.
  [[nodiscard]] static Rng fromSeed(std::uint64_t seed);

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

  [[nodiscard]] std::int64_t uniformInt(std::int64_t low, std::int64_t high);

  [[nodiscard]] double nextDouble() noexcept { return engine_.next_double(); }
  [[nodiscard]] std::uint64_t nextU64() noexcept { return engine_.next_u64(); }

  // ----- Collection samplers -----

  /// Pick one index uniformly from [0, n).
  [[nodiscard]] std::size_t choiceIndex(std::size_t n);

  /// Pick k indices from [0, n). Without replacement by default.
  [[nodiscard]] std::vector<std::size_t>
  choiceIndices(std::size_t n, std::size_t k, bool replace = false);

private:
  /// Lemire's nearly-divisionless bounded random.
  [[nodiscard]] std::uint64_t bounded(std::uint64_t range) noexcept;

  Pcg64 engine_;
};

} // namespace PhantomLedger::random
