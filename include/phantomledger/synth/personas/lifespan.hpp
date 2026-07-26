#pragma once
//
// phantomledger/synth/personas/lifespan.hpp
//
// macro-history-v1 H3 step 1: the LIFESPAN primitive. Every person
// gets a deterministic DEATH DATE, derived once on the isolated
// {"mortality", personId} lane from the single-age-axis birth date
// (Pack::birthDates) and the embedded SSA 2023 period life table
// (synth/econ/catalog.hpp mortality() — table 4.C6, sex-specific
// annual qx, log-linear age interpolation; pinned by
// test_econ_catalog and unread by generation until this arc).
//
// THE DEFECT THIS ARC CLOSES: nobody dies. A 29-year canonical
// window carries retirees seeded 65-99 who would reach 94-128, an
// inheritance HAZARD detached from any death event, and a population
// that only ever grows (membership is joiners-only). H3 wires death:
// step 2 clips income/spending/benefits at the death date, replaces
// the hazard inheritance with death-caused estates (NFDA funeral
// cost, estate distribution over the family graph), closes the
// membership interval at [joinTs, closeTs), and lets the freed
// capacity replenish through join cohorts.
//
// ALIVE-AT-START INVARIANT (the mortality analog of H2's
// personaAt(simStart)==seed): everyone on the sim-start roster is
// alive at sim start BY DEFINITION, so the hazard walk begins at the
// person's CURRENT age — the drawn death is conditional on survival
// to sim start and lands strictly after it (floored at one day in).
//
// DECLARED SIMPLIFICATIONS (authority rows land with the H3 wiring
// merge):
//   - SEX is a LATENT mortality attribute drawn 50/50 on the same
//     lane (the world models no sex; the table's ~2.7-year male/
//     female gap is real signal worth keeping — surfacing sex to PII
//     and using a measured population ratio is a registered upgrade).
//   - ONE period table era-wide (2023), like the H1 one-index-per-
//     axis choice; historical-period mortality is a registered
//     upgrade.
//   - NO persona-differential mortality (SES gradients are a
//     registered upgrade).
//   - Deaths distribute UNIFORMLY within the death year.
//
// DRAW DISCIPLINE: exactly THREE draws per person, unconditional, in
// a fixed documented order, all on the person's own lane — entity N
// can never move entity N+1; adding a fourth draw later is an
// explicit model change.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/econ/catalog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace PhantomLedger::synth::personas::lifespan {

// The table's practical ceiling: the hazard walk stops here and any
// residual survival mass dies at the cap (SSA's table runs to ~119;
// the interpolation clamps beyond its last pivot).
inline constexpr double kMaxAgeYears = 120.0;

inline constexpr double kDaysPerYear = 365.2425;

/// One person's lifespan. `death` is strictly after sim start for
/// everyone on the sim-start roster (see the invariant above).
struct Lifespan {
  bool male = false; // latent mortality attribute (declared)
  time::TimePoint death{};
};

struct Inputs {
  entity::PersonId person = 0;
  time::CalendarDate dob{};
  time::TimePoint simStart{};
};

namespace detail {

inline void requireInputs(const Inputs &in) {
  if (in.person == 0) {
    throw std::invalid_argument("lifespan::derive: person must be >= 1");
  }
  if (in.dob.year < 1850 || in.dob.year > 2200) {
    throw std::invalid_argument("lifespan::derive: dob.year out of range");
  }
  if (in.dob.month < 1 || in.dob.month > 12) {
    throw std::invalid_argument("lifespan::derive: dob.month out of range");
  }
  if (in.dob.day < 1 || in.dob.day > 31) {
    throw std::invalid_argument("lifespan::derive: dob.day out of range");
  }
}

[[nodiscard]] inline double ageYearsAt(time::CalendarDate dob,
                                       time::TimePoint at) {
  const auto dobTs = time::makeTime(time::CalendarDate{
      .year = dob.year, .month = dob.month, .day = dob.day});
  const double seconds = static_cast<double>(time::toEpochSeconds(at) -
                                             time::toEpochSeconds(dobTs));
  return seconds / (kDaysPerYear * 86'400.0);
}

// Remaining years until death, conditional on being alive at
// fractional age a0: an annual hazard walk over the interpolated
// sex-specific qx, inverted at the single uniform uLife; uFrac
// places the death uniformly inside its year. Pure — the test
// recomputes the expectation from the same table independently.
[[nodiscard]] inline double remainingYears(const econ::MortalityTable &table,
                                           bool male, double a0, double uLife,
                                           double uFrac) noexcept {
  double survival = 1.0;
  double cum = 0.0;
  double k = 0.0;

  while (a0 + k < kMaxAgeYears) {
    const double age = a0 + k;
    const double qx =
        std::clamp(male ? table.qxMale(age) : table.qxFemale(age), 0.0, 1.0);
    const double dieThisYear = survival * qx;

    if (uLife < cum + dieThisYear) {
      return k + uFrac;
    }

    cum += dieThisYear;
    survival *= (1.0 - qx);
    k += 1.0;
  }

  // Residual mass beyond the ceiling dies at the cap.
  return std::max(0.0, kMaxAgeYears - a0) + uFrac;
}

} // namespace detail

[[nodiscard]] inline Lifespan derive(const random::RngFactory &factory,
                                     const Inputs &in) {
  detail::requireInputs(in);

  auto rng = factory.rng({"mortality", std::to_string(in.person)});

  // The three draws, unconditional, in this order (contract):
  const double uSex = rng.nextDouble();  // 1
  const double uLife = rng.nextDouble(); // 2
  const double uFrac = rng.nextDouble(); // 3

  const bool male = uSex < 0.50;
  const double a0 = std::max(0.0, detail::ageYearsAt(in.dob, in.simStart));

  const double t = detail::remainingYears(econ::mortality(), male, a0, uLife,
                                          uFrac);

  // Anchor the death to the BIRTH date (durability: life events, not
  // window events), then enforce the alive-at-start invariant against
  // day rounding: death lands at least one day into the window.
  const auto dobTs = time::makeTime(time::CalendarDate{
      .year = in.dob.year, .month = in.dob.month, .day = in.dob.day});
  const auto deathDays =
      static_cast<int>(std::lround((a0 + t) * kDaysPerYear));

  auto death = time::addDays(dobTs, deathDays);
  const auto floorTs = time::addDays(in.simStart, 1);
  if (death < floorTs) {
    death = floorTs;
  }

  return Lifespan{.male = male, .death = death};
}

/// Alive strictly before the death instant.
[[nodiscard]] inline bool aliveAt(const Lifespan &ls,
                                  time::TimePoint at) noexcept {
  return at < ls.death;
}

/// Every person's lifespan from the single-age carrier
/// (Pack::birthDates), on the same world-seed factory the dob and
/// persona-era lanes use. Mortality is persona-independent (declared),
/// so no assignment is consumed.
[[nodiscard]] inline std::vector<Lifespan>
deriveAll(std::uint64_t worldSeed, time::TimePoint simStart,
          const std::vector<time::CalendarDate> &birthDates) {
  const random::RngFactory factory{worldSeed};
  std::vector<Lifespan> out;
  out.reserve(birthDates.size());
  for (std::size_t idx = 0; idx < birthDates.size(); ++idx) {
    out.push_back(derive(factory, Inputs{
                                      .person = static_cast<entity::PersonId>(
                                          idx + 1),
                                      .dob = birthDates[idx],
                                      .simStart = simStart,
                                  }));
  }
  return out;
}

} // namespace PhantomLedger::synth::personas::lifespan
