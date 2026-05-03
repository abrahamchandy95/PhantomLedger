#pragma once

#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/spending/market/commerce/contacts.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>

namespace PhantomLedger::relationships::social {

struct Social {
  int targetDegree = 12;
  double localProb = 0.70;
  double hubMultiplier = 25.0;
  double influenceSigma = 1.1;
  double tieStrengthShape = 1.0;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::gt("targetDegree", targetDegree, 0); });
    r.check([&] { v::between("localProb", localProb, 0.0, 1.0); });
    r.check([&] { v::gt("hubMultiplier", hubMultiplier, 0.0); });
    r.check([&] { v::gt("influenceSigma", influenceSigma, 0.0); });
    r.check([&] { v::gt("tieStrengthShape", tieStrengthShape, 0.0); });
  }

  /// Clamp `targetDegree` into the supported range.
  [[nodiscard]] constexpr int effectiveDegree() const noexcept {
    return std::clamp(targetDegree, 3, 24);
  }

  /// Cross-community pick probability. Always in [0.01, 0.25].
  [[nodiscard]] constexpr double crossProb() const noexcept {
    return std::clamp(1.0 - localProb, 0.01, 0.25);
  }

  /// Community-block size bounds. Bounds scale with effective
  /// degree so larger graphs naturally form larger communities.
  [[nodiscard]] constexpr std::pair<int, int> communityBounds() const noexcept {
    const int k = effectiveDegree();
    const int cMin = std::max(20, 6 * k);
    const int cMax = std::max(cMin + 20, 24 * k);
    return {cMin, cMax};
  }
};

inline constexpr Social kDefaultSocial{};

struct BuildInputs {
  std::uint32_t personCount = 0;
  std::span<const std::uint8_t> hubFlags;
  std::uint64_t baseSeed = 0;
};

[[nodiscard]] spending::market::commerce::Contacts
build(const Social &social, const BuildInputs &inputs);

} // namespace PhantomLedger::relationships::social
