#pragma once
//
// phantomledger/synth/personas/join.hpp
//
// macro-history-v1 H3 part 3c-ii: the JOIN COHORT (authority U-8
// addendum). THE DEFECT THIS CLOSES: membership was joiners-only at a
// flat 2%/yr with hash-placed join days, joiners aged as of SIM START
// (the declared JOINER AGE AXIS ERROR), and nothing ever closed — the
// population could die (H3 parts 1-3b) but never replenish.
//
// SIZING (MEASUREMENT + CHOICE): the bank's customer base tracks
// U.S. resident-population growth —
//
//   joinerCount = population x SUM over window days of
//                 r(year(day)) / 365.2425          (linear, declared)
//
// where r(y) = pop(y+1)/pop(y) - 1 from the EMBEDDED BEA NIPA
// population series (synth/econ/era_data.hpp, U-4/U-5 lineage).
// RATE-CLAMPED at coverage edges: a year outside coverage reads the
// nearest MEASURED year-over-year rate — the rate-axis analog of the
// H1 level freeze (never extrapolated, never wall-clock).
//
// PLACEMENT (INVARIANT): joiners are the LAST K person ids (the seed
// roster's dob/persona-era/mortality draws stay byte-identical), and
// each joiner's join day is EXACTLY ONE draw on the isolated
// {"join-cohort", personId} lane — inverse-CDF over per-day weights
// proportional to r(year(day)), so high-growth years recruit more.
// Entity N cannot move entity N+1; a second draw is a model change.
//
// THE AGE AXIS: Pack::joinDays feeds the dob/timeline anchors (a
// joiner's ages draw AT THE JOIN DATE — dob.hpp, timeline.hpp), and
// membershipOf() below is the ONE Membership construction path every
// exporter shares, so the visible corpus and the world cannot
// diverge.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/econ/catalog.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/pii/membership.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::personas::join_cohort {

inline constexpr double kDaysPerYear = 365.2425;

namespace detail {

// The population growth rate consumed for calendar year `year`:
// pop(a+1)/pop(a) - 1 with a clamped so both endpoints are measured.
// Negative rates clamp to zero (the embedded series only rises; the
// guard keeps a future series edit from producing negative cohorts).
[[nodiscard]] inline double annualGrowthRate(int year) {
  const auto &m = econ::macroSeries();
  const int a = std::clamp(year, m.firstYear(), m.lastYear() - 1);
  const double p0 = m.at(a).populationThousands;
  const double p1 = m.at(a + 1).populationThousands;
  if (p0 <= 0.0) {
    return 0.0;
  }
  return std::max(0.0, p1 / p0 - 1.0);
}

// Per-day join weights across the window: w_d = r(year of day d).
[[nodiscard]] inline std::vector<double>
dayWeights(const time::Window &window) {
  std::vector<double> out;
  if (window.days <= 0) {
    return out;
  }
  out.reserve(static_cast<std::size_t>(window.days));
  int cachedYear = std::numeric_limits<int>::min();
  double cachedRate = 0.0;
  for (int d = 0; d < window.days; ++d) {
    const int year =
        time::toCalendarDate(window.start + time::Days{d}).year;
    if (year != cachedYear) {
      cachedYear = year;
      cachedRate = annualGrowthRate(year);
    }
    out.push_back(cachedRate);
  }
  return out;
}

} // namespace detail

/// The BEA-sized join-cohort count (see the file comment). Capped at
/// the population; zero for empty windows/populations.
[[nodiscard]] inline std::size_t joinerCount(std::size_t population,
                                             const time::Window &window) {
  if (population == 0 || window.days <= 0) {
    return 0;
  }
  const auto weights = detail::dayWeights(window);
  double sum = 0.0;
  for (const double w : weights) {
    sum += w;
  }
  const double expected =
      static_cast<double>(population) * (sum / kDaysPerYear);
  const auto n = static_cast<std::size_t>(
      std::max<long long>(0, std::llround(expected)));
  return std::min(n, population);
}

/// Every person's join-day offset from window start (PersonId-1
/// indexed): 0 for the seed roster; a drawn day for the join cohort
/// (the LAST joinerCount ids), exactly ONE draw per joiner on the
/// isolated {"join-cohort", personId} lane. Fills Pack::joinDays.
[[nodiscard]] inline std::vector<std::uint32_t>
deriveJoinDays(std::uint64_t worldSeed, std::size_t population,
               const time::Window &window) {
  std::vector<std::uint32_t> out(population, 0U);
  const auto joiners = joinerCount(population, window);
  if (joiners == 0) {
    return out;
  }

  const auto weights = detail::dayWeights(window);
  std::vector<double> cdf;
  cdf.reserve(weights.size());
  double total = 0.0;
  for (const double w : weights) {
    total += w;
    cdf.push_back(total);
  }

  const random::RngFactory factory{worldSeed};
  const auto firstJoiner = population - joiners + 1;
  for (std::size_t person = firstJoiner; person <= population; ++person) {
    auto rng = factory.rng({"join-cohort", std::to_string(person)});
    const double u = rng.nextDouble(); // THE one draw (contract)

    std::size_t day;
    if (total > 0.0) {
      const auto it =
          std::upper_bound(cdf.begin(), cdf.end(), u * total);
      day = static_cast<std::size_t>(it - cdf.begin());
    } else {
      day = static_cast<std::size_t>(u * static_cast<double>(window.days));
    }
    day = std::min(day, static_cast<std::size_t>(window.days - 1));
    out[person - 1] = static_cast<std::uint32_t>(day);
  }

  return out;
}

/// Per-person ACCOUNT-CLOSURE epochs: death + kSettlementDays
/// (PersonId-1 indexed; pii::Membership's close bound).
[[nodiscard]] inline std::vector<std::int64_t>
closeEpochsOf(const std::vector<timeline::Timeline> &timelines) {
  std::vector<std::int64_t> out;
  out.reserve(timelines.size());
  for (const auto &tl : timelines) {
    out.push_back(time::toEpochSeconds(tl.death) +
                  static_cast<std::int64_t>(pii::kSettlementDays) * 86'400);
  }
  return out;
}

/// THE one Membership construction path (every exporter and main
/// share it, so the visible corpus cannot diverge). Guard shapes: a
/// pack missing the join carrier yields no joiners; one missing the
/// timeline carrier yields no closures — each half degrades to its
/// pre-3c-ii behavior independently.
[[nodiscard]] inline pii::Membership membershipOf(const Pack &pack,
                                                  const time::Window &window) {
  const auto population = pack.assignment.byPerson.size();

  auto joinDays = pack.joinDays;
  if (joinDays.size() != population) {
    joinDays.assign(population, 0U);
  }

  auto closes = closeEpochsOf(pack.timelines);
  if (closes.size() != population) {
    closes.assign(population, std::numeric_limits<std::int64_t>::max());
  }

  return pii::Membership{window, std::move(joinDays), std::move(closes)};
}

} // namespace PhantomLedger::synth::personas::join_cohort
