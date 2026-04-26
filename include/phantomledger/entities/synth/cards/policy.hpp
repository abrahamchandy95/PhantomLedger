#pragma once

#include <cstdint>

namespace PhantomLedger::entities::synth::cards {

struct Policy {
  // ---- APR distribution ----

  double aprMedian = 0.22;
  double aprSigma = 0.25;
  double aprMin = 0.08;
  double aprMax = 0.36;

  // ---- Credit-limit distribution ----

  double limitSigma = 0.65;
  double limitFloor = 200.0;

  // ---- Cycle day ----

  std::uint8_t cycleDayMin = 1;
  std::uint8_t cycleDayMax = 28;

  // ---- Autopay categorical ----

  double autopayFullP = 0.40;
  double autopayMinP = 0.10;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return aprMedian > 0.0 && aprSigma >= 0.0 && aprMin > 0.0 &&
           aprMax >= aprMin && limitSigma > 0.0 && limitFloor >= 0.0 &&
           cycleDayMin >= 1 && cycleDayMax >= cycleDayMin &&
           cycleDayMax <= 28 && autopayFullP >= 0.0 && autopayFullP <= 1.0 &&
           autopayMinP >= 0.0 && autopayMinP <= 1.0 &&
           (autopayFullP + autopayMinP) <= 1.0;
  }
};

inline constexpr Policy kDefaultPolicy{};

} // namespace PhantomLedger::entities::synth::cards
