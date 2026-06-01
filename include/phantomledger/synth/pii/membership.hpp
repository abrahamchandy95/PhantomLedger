#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::synth::pii {

struct Growth {
  double annualRate = 0.02;

  bool enabled = true;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::nonNegative("growth.annualRate", annualRate); });
    r.check([&] { v::between("growth.annualRate", annualRate, 0.0, 1.0); });
  }

  [[nodiscard]] std::size_t joinerCount(std::size_t population,
                                        const time::Window &window) const {
    if (!enabled || population == 0 || window.days <= 0) {
      return 0;
    }
    const double frac = annualRate * (static_cast<double>(window.days) / 365.0);
    auto n =
        static_cast<std::size_t>(static_cast<double>(population) * frac + 0.5);
    return n > population ? population : n;
  }
};

class Membership {
public:
  Membership(std::size_t population, const time::Window &window,
             const Growth &growth) noexcept
      : window_(window), enabled_(growth.enabled),
        firstJoinerId_(computeFirstJoinerId(population, window, growth)) {}

  [[nodiscard]] bool isJoiner(entity::PersonId person) const noexcept {
    return enabled_ && firstJoinerId_ != 0 && person >= firstJoinerId_;
  }

  [[nodiscard]] time::TimePoint joinTs(entity::PersonId person) const noexcept {
    if (!isJoiner(person)) {
      return window_.start;
    }
    const int days = window_.days > 0 ? window_.days : 1;
    std::uint64_t z =
        static_cast<std::uint64_t>(person) + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);
    const auto dayIn = static_cast<int>(z % static_cast<std::uint64_t>(days));
    return window_.start + time::Days{dayIn};
  }

  [[nodiscard]] bool activeAt(entity::PersonId person,
                              std::int64_t ts) const noexcept {
    if (!isJoiner(person)) {
      return true;
    }
    return ts >= time::toEpochSeconds(joinTs(person));
  }

private:
  [[nodiscard]] static entity::PersonId
  computeFirstJoinerId(std::size_t population, const time::Window &window,
                       const Growth &growth) noexcept {
    const auto joiners = growth.joinerCount(population, window);
    if (joiners == 0) {
      return 0; // sentinel: no joiners
    }
    return static_cast<entity::PersonId>(population - joiners + 1);
  }

  time::Window window_;
  bool enabled_;
  entity::PersonId firstJoinerId_;
};

} // namespace PhantomLedger::synth::pii
