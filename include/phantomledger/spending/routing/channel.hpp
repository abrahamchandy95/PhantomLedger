#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::spending::routing {

enum class Slot : std::uint8_t {
  merchant = 0,
  bill = 1,
  p2p = 2,
  externalUnknown = 3,
  kCount = 4,
};

inline constexpr std::size_t kSlotCount =
    static_cast<std::size_t>(Slot::kCount);

using ChannelCdf = std::array<double, kSlotCount>;

[[nodiscard]] constexpr ChannelCdf
buildChannelCdf(double merchantP, double billsP, double p2pP,
                double unknownOutflowP) noexcept {
  const double unknown = std::clamp(unknownOutflowP, 0.0, 1.0);

  double core0 = merchantP;
  double core1 = billsP;
  double core2 = p2pP;
  double coreSum = core0 + core1 + core2;

  if (!(coreSum > 0.0)) {
    core0 = core1 = core2 = 1.0;
    coreSum = 3.0;
  }

  const double scale = (1.0 - unknown) / coreSum;
  const double s0 = core0 * scale;
  const double s1 = core1 * scale;
  const double s2 = core2 * scale;

  ChannelCdf cdf{};
  cdf[0] = s0;
  cdf[1] = cdf[0] + s1;
  cdf[2] = cdf[1] + s2;
  cdf[3] = cdf[2] + unknown;
  return cdf;
}

[[nodiscard]] inline Slot pickSlot(const ChannelCdf &cdf, double u) noexcept {
  // Unrolled — branch predictor handles the four-way ladder cleanly.
  if (u < cdf[0]) {
    return Slot::merchant;
  }
  if (u < cdf[1]) {
    return Slot::bill;
  }
  if (u < cdf[2]) {
    return Slot::p2p;
  }
  return Slot::externalUnknown;
}

} // namespace PhantomLedger::spending::routing
