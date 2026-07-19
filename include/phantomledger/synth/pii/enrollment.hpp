#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::synth::pii {

struct Enrollment {
  double foundingFraction = 0.85;

  int foundingMinDaysBefore = 90;
  int foundingMaxDaysBefore = 365 * 5;

  double annualNewAccountRate = 0.18;

  bool enabled = true;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::unit("enrollment.foundingFraction", foundingFraction); });
    r.check([&] {
      v::nonNegative("enrollment.foundingMinDaysBefore", foundingMinDaysBefore);
    });
    r.check([&] {
      v::ge("enrollment.foundingMaxDaysBefore", foundingMaxDaysBefore,
            foundingMinDaysBefore);
    });
    r.check([&] {
      v::nonNegative("enrollment.annualNewAccountRate", annualNewAccountRate);
    });
  }
};

[[nodiscard]] inline time::TimePoint createdAtFor(entity::PersonId person,
                                                  const time::Window &window,
                                                  const Enrollment &cfg) {
  if (!cfg.enabled) {
    return window.start;
  }

  std::uint64_t z = static_cast<std::uint64_t>(person) + 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  z = z ^ (z >> 31);

  const double u0 =
      static_cast<double>(z & 0xFFFFFFFFULL) / 4294967296.0; // [0,1)
  const double u1 =
      static_cast<double>((z >> 32) & 0xFFFFFFFFULL) / 4294967296.0; // [0,1)

  if (u0 < cfg.foundingFraction) {
    const int span = cfg.foundingMaxDaysBefore - cfg.foundingMinDaysBefore + 1;
    const int daysBefore = cfg.foundingMinDaysBefore +
                           static_cast<int>(u1 * static_cast<double>(span));
    return window.start - time::Days{daysBefore};
  }

  const int days = window.days > 0 ? window.days : 1;
  const int dayIn = static_cast<int>(u1 * static_cast<double>(days));
  return window.start + time::Days{dayIn};
}

} // namespace PhantomLedger::synth::pii
