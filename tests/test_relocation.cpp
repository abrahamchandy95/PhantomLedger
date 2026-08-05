// tests/test_relocation.cpp
//
// THE RELOCATION GATE (relocation-2026-07).
//
// The defect: `pii::Address::geoArea` was assigned once and fixed for the
// run, so a 20-year corpus contained ZERO relocation while Census CPS ASEC
// puts the annual mover rate at 15.9% (1998-99) declining to 8.4% (2021).
// The pre-round state scores EXACTLY 0 on check B, which is how this gate
// detects it.
//
// WHY IT MEASURES THE SCHEDULE DIRECTLY. The quantity under test is a
// per-person COUNT and a composition, and the corpus only shows those through
// their effect on merchant selection — which the live merchant set also moves.
// Measuring the rule keeps the gate pointed at the mechanism, the same
// reasoning `test_card_churn` and the burst gate use.
//
// The COMPLEMENTARY corpus-level property — that the home the exporter
// publishes is the home the fold actually selected against — is gated in
// `test_pipeline_e2e` where the card view already exists. Splitting them that
// way is deliberate: this file must stay runnable without an exporter.
//
// WHAT IS CHECKED, and why each earns its place:
//
//   A. TENURES TILE, ASCENDING, INSIDE THE WINDOW, WITH NO REPEATED AREA.
//      Tenure 0 must start at the window start and equal the `homeAreas`
//      snapshot byte for byte — that identity is what makes a zero-move
//      window reproduce the pre-round corpus. Consecutive areas must DIFFER,
//      or a no-op move becomes a spurious exported edge.
//
//   B. THE REALIZED MOVE RATE TRACKS THE CPS SERIES, per person-year, against
//      `moverRateFor(year) * kAreaChangingShare`.
//
//   C. THE ERA DECLINE IS REAL. The first half of a long window must carry a
//      higher rate than the second. A FLAT hazard passes B and fails this,
//      and the entire reason for reading CPS rather than picking a number was
//      that mobility has fallen for four decades.
//
//   D. CORESIDENTS MOVE TOGETHER — compared against the REAL household
//      partition, not a proxy. `party-geography-2026-07` declares coresident
//      home identity binding; a per-person schedule would silently falsify it.
//
//   E. DESTINATIONS RESPECT THE CPS COMPOSITION and stay in-country. The
//      same-state share is banded; a cross-country move is a HARD ZERO.
//
//   F. SHORT WINDOWS STAY QUIET, which is what pins the SCALING. A per-run
//      coin and a per-year rate are indistinguishable at one horizon — the
//      exact defect `burst-rate-2026-07` found after it had survived every
//      prior round.

#include "test_support.hpp"

#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/geo/catalog.hpp"
#include "phantomledger/synth/pii/make.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

namespace pl = ::PhantomLedger;
namespace reloc = ::PhantomLedger::entity::parties::relocation;

int g_failures = 0;

void check(bool cond, const std::string &what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what.c_str());
    ++g_failures;
  }
}

// ------------------------------------------------------------- bands
//
// B is DERIVED from the CPS anchors, and the derivation is written out so a
// later reader can separate a calibration change from a construction defect.
// Over a window spanning 2000-2020 the mean of `moverRateFor(year)` is ~0.121
// and the area-changing rate is ~0.85 of that, so the nominal is ~0.103
// moves/person-year. The REALIZED value sits below it because a destination
// redraw landing on the origin area is dropped as a no-op.
constexpr double kMinMoveRate = 0.070;
constexpr double kMaxMoveRate = 0.115;

// E: `kSameStateShare` is 0.818 nominal, but it applies only when the origin's
// state has residential areas in the pinned catalogue, and a single-area state
// makes the in-state redraw a dropped no-op. The floor is what a model
// ignoring state entirely would fail — that scores roughly the largest state's
// share of national population.
constexpr double kMinSameStateShare = 0.40;

// F: over 60 days the annual rate can barely fire.
constexpr double kMaxShortWindowMovesPerPerson = 0.05;

struct Leg {
  const char *name;
  int startYear;
  std::int32_t days;
  std::int32_t population;
  std::uint64_t seed;
};

[[nodiscard]] pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  const pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

[[nodiscard]] pl::time::Window windowOf(const Leg &leg) {
  return pl::time::Window{
      .start = pl::time::makeTime({leg.startYear, 1, 1}),
      .days = leg.days,
  };
}

// World build only — no transfer fold. The schedule is entity-stage state, and
// folding a 20-year 400-person corpus to read it would cost minutes for
// nothing.
[[nodiscard]] pl::pipeline::SimulationResult
buildWorld(const pl::synth::pii::PoolSet &poolSet, const Leg &leg) {
  const auto window = windowOf(leg);
  const pl::pipeline::stages::entities::EntitySynthesis entities{
      .population = leg.population,
      .identity =
          pl::synth::pii::IdentityContext{
              .pools = &poolSet,
              .simStart = window.start,
              .windowDays = leg.days,
              .localeMix = pl::synth::pii::LocaleMix::usOnly(),
          },
  };
  auto rng = pl::random::Rng::fromSeed(leg.seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entities, leg.seed};
  return pipeline.buildWorld();
}

struct Measurement {
  std::size_t people = 0;
  std::size_t moves = 0;
  double personYears = 0.0;
  std::size_t structuralFailures = 0;
  std::size_t repeatedArea = 0;
  std::size_t snapshotMismatch = 0;
  std::size_t crossCountry = 0;
  std::size_t sameStateMoves = 0;
  std::size_t statedMoves = 0;
  double firstHalfRate = 0.0;
  double secondHalfRate = 0.0;
};

[[nodiscard]] Measurement
measureSchedule(const reloc::Schedule &schedule,
                const std::vector<pl::entity::geography::GeoAreaId> &initial,
                pl::time::Window window) {
  Measurement out;
  const auto &catalog = pl::synth::geo::geography();
  const auto windowStart = pl::time::toEpochSeconds(window.start);
  const auto windowEnd =
      pl::time::toEpochSeconds(pl::time::addDays(window.start, window.days));
  const auto midpoint = windowStart + (windowEnd - windowStart) / 2;

  std::size_t firstHalfMoves = 0;
  std::size_t secondHalfMoves = 0;

  out.people = schedule.personCount();
  for (std::size_t i = 0; i < out.people; ++i) {
    const auto person = static_cast<pl::entity::PersonId>(i + 1);
    const auto rows = schedule.tenures(person);
    if (rows.empty()) {
      ++out.structuralFailures;
      continue;
    }

    if (rows.front().fromEpoch != windowStart) {
      ++out.structuralFailures;
    }
    if (i < initial.size() && rows.front().area != initial[i]) {
      ++out.snapshotMismatch;
    }

    for (std::size_t r = 1; r < rows.size(); ++r) {
      if (rows[r].fromEpoch <= rows[r - 1].fromEpoch ||
          rows[r].fromEpoch <= windowStart || rows[r].fromEpoch >= windowEnd) {
        ++out.structuralFailures;
      }
      if (rows[r].area == rows[r - 1].area) {
        ++out.repeatedArea;
      }

      ++out.moves;
      if (rows[r].fromEpoch < midpoint) {
        ++firstHalfMoves;
      } else {
        ++secondHalfMoves;
      }

      const auto from = rows[r - 1].area;
      const auto to = rows[r].area;
      if (catalog.contains(from) && catalog.contains(to)) {
        const auto &a = catalog.at(from);
        const auto &b = catalog.at(to);
        if (a.country != b.country) {
          ++out.crossCountry;
        }
        if (!a.stateCode.empty() && !b.stateCode.empty()) {
          ++out.statedMoves;
          if (a.stateCode == b.stateCode) {
            ++out.sameStateMoves;
          }
        }
      }
    }
  }

  const auto years =
      static_cast<double>(windowEnd - windowStart) / (365.25 * 86'400.0);
  out.personYears = static_cast<double>(out.people) * years;
  const auto halfPersonYears = out.personYears / 2.0;
  out.firstHalfRate =
      halfPersonYears > 0.0
          ? static_cast<double>(firstHalfMoves) / halfPersonYears
          : 0.0;
  out.secondHalfRate =
      halfPersonYears > 0.0
          ? static_cast<double>(secondHalfMoves) / halfPersonYears
          : 0.0;
  return out;
}

} // namespace

int main() {
  try {
    // ---------------------------------------------- leg 1: the target shape
    const Leg longLeg{"leg-20y", 2000, 7305, 400, 20260730ULL};
    const auto longPools = buildPoolSet(longLeg.seed);
    const auto longWorld = buildWorld(longPools, longLeg);
    const auto longWindow = windowOf(longLeg);
    const auto &schedule = longWorld.people.relocation;
    const auto m = measureSchedule(schedule, longWorld.people.homeAreas,
                                   longWindow);
    const auto rate =
        m.personYears > 0.0 ? static_cast<double>(m.moves) / m.personYears : 0.0;
    const auto sameStateShare =
        m.statedMoves == 0 ? 0.0
                           : static_cast<double>(m.sameStateMoves) /
                                 static_cast<double>(m.statedMoves);

    std::printf("=== %s: pop %d, %d days from %d ===\n", longLeg.name,
                longLeg.population, longLeg.days, longLeg.startYear);
    std::printf("  %zu people, %zu moves, %.1f person-years => %.4f "
                "moves/person-year\n",
                m.people, m.moves, m.personYears, rate);
    std::printf("  era decline: first half %.4f, second half %.4f\n",
                m.firstHalfRate, m.secondHalfRate);
    std::printf("  composition: %zu stated moves, %zu same-state (%.4f), %zu "
                "cross-country\n",
                m.statedMoves, m.sameStateMoves, sameStateShare,
                m.crossCountry);
    // WHY THE REALIZED SAME-STATE SHARE SITS BELOW THE 0.818 NOMINAL, printed
    // so the gap is auditable rather than mysterious. `sampleExcluding` falls
    // back to a country-wide draw when the origin's state has fewer than two
    // residential areas — and the pinned 71-city catalogue leaves many states
    // with exactly one. Those in-state intentions become out-of-state moves.
    // A catalogue limitation, not a model defect: the shortfall shrinks as the
    // catalogue grows, and the check below is a floor for that reason.
    {
      std::map<std::string, std::size_t> areasPerState;
      for (const auto &area : pl::synth::geo::geography().areas()) {
        if (area.population > 0 && !area.stateCode.empty()) {
          ++areasPerState[area.stateCode];
        }
      }
      std::size_t singles = 0;
      for (const auto &[state, count] : areasPerState) {
        if (count < 2) {
          ++singles;
        }
      }
      std::printf("  catalogue: %zu states with residential areas, %zu of them "
                  "single-area (an in-state move there falls back to "
                  "country-wide)\n",
                  areasPerState.size(), singles);
    }
    std::printf("  distinct areas ever occupied: %zu (window-start distinct "
                "%zu)\n",
                schedule.allAreas().size(), [&] {
                  auto copy = longWorld.people.homeAreas;
                  std::ranges::sort(copy);
                  copy.erase(std::ranges::unique(copy).begin(), copy.end());
                  return copy.size();
                }());

    // A
    check(m.structuralFailures == 0,
          "leg-20y: tenures must tile ascending INSIDE the window (" +
              std::to_string(m.structuralFailures) + " violations)");
    check(m.snapshotMismatch == 0,
          "leg-20y: tenure 0 must equal the homeAreas snapshot byte for byte "
          "(" +
              std::to_string(m.snapshotMismatch) +
              " mismatches). That identity is what makes a zero-move window "
              "reproduce the pre-round corpus exactly");
    check(m.repeatedArea == 0,
          "leg-20y: consecutive tenures must have DIFFERENT areas (" +
              std::to_string(m.repeatedArea) +
              " repeats). A same-area move is invisible at this granularity "
              "and would export as a spurious edge");
    // B
    check(rate >= kMinMoveRate && rate <= kMaxMoveRate,
          "leg-20y: moves per person-year must sit in [" +
              std::to_string(kMinMoveRate) + ", " +
              std::to_string(kMaxMoveRate) + "], got " + std::to_string(rate) +
              ". The pre-round state is EXACTLY 0 — nobody ever moved");
    // C
    check(m.firstHalfRate > m.secondHalfRate,
          "leg-20y: the CPS era DECLINE must be visible — first-half rate " +
              std::to_string(m.firstHalfRate) +
              " must exceed second-half " + std::to_string(m.secondHalfRate) +
              ". A FLAT hazard passes the level check and fails this one");
    // E
    check(m.crossCountry == 0,
          "leg-20y: a relocation must stay inside the origin's country (" +
              std::to_string(m.crossCountry) +
              " cross-country moves). Country drives locale, PII format and "
              "the whole identity layer, none of which move mid-run");
    check(m.statedMoves > 0,
          "leg-20y: some move must have a state on both ends, or the "
          "composition check is vacuous");
    check(sameStateShare >= kMinSameStateShare,
          "leg-20y: the CPS same-state composition must survive into realized "
          "moves — share " +
              std::to_string(sameStateShare) + " below floor " +
              std::to_string(kMinSameStateShare) +
              ". A model ignoring state scores roughly the largest state's "
              "population share");

    // ------------------------------------------------------- D coresidence
    //
    // Against the REAL partition, reproduced the same way home placement and
    // the schedule builder both do. A proxy (say, "same initial area and same
    // first move date") would beg the question.
    const auto households = pl::synth::pii::reproduceHouseholds(
        longLeg.seed, longWorld.people.personas.assignment);
    std::map<std::pair<std::uint32_t, pl::entity::geography::GeoAreaId>,
             std::vector<pl::entity::PersonId>>
        groups;
    for (std::size_t i = 0; i < schedule.personCount(); ++i) {
      const auto household = i < households.householdOf.size()
                                 ? households.householdOf[i]
                                 : static_cast<std::uint32_t>(i);
      groups[{household, longWorld.people.homeAreas[i]}].push_back(
          static_cast<pl::entity::PersonId>(i + 1));
    }
    std::size_t comparedGroups = 0;
    std::size_t divergent = 0;
    std::size_t movingGroups = 0;
    for (const auto &[key, members] : groups) {
      if (members.size() < 2) {
        continue;
      }
      ++comparedGroups;
      const auto reference = schedule.tenures(members.front());
      if (reference.size() > 1) {
        ++movingGroups;
      }
      for (std::size_t k = 1; k < members.size(); ++k) {
        const auto other = schedule.tenures(members[k]);
        if (other.size() != reference.size()) {
          ++divergent;
          continue;
        }
        for (std::size_t r = 0; r < reference.size(); ++r) {
          if (reference[r].fromEpoch != other[r].fromEpoch ||
              reference[r].area != other[r].area) {
            ++divergent;
            break;
          }
        }
      }
    }
    std::printf("\n=== coresidence ===\n");
    std::printf("  %zu multi-member coresident groups, %zu of them move, %zu "
                "divergent\n",
                comparedGroups, movingGroups, divergent);
    check(comparedGroups > 0,
          "coresidence: the harness must produce multi-member households, or "
          "check D is vacuous");
    check(movingGroups > 0,
          "coresidence: some multi-member household must MOVE, or D passes on "
          "households that never had a chance to diverge");
    check(divergent == 0,
          "coresidence: members of one household sharing an initial area must "
          "have IDENTICAL tenure sequences (" +
              std::to_string(divergent) +
              " divergent). party-geography-2026-07 declares coresident home "
              "identity binding");

    // -------------------------------------------------- F: the short window
    const Leg shortLeg{"leg-60d", 2000, 60, 400, 20260731ULL};
    const auto shortPools = buildPoolSet(shortLeg.seed);
    const auto shortWorld = buildWorld(shortPools, shortLeg);
    const auto shortMeasure =
        measureSchedule(shortWorld.people.relocation,
                        shortWorld.people.homeAreas, windowOf(shortLeg));
    const double shortPerPerson =
        shortMeasure.people == 0
            ? 0.0
            : static_cast<double>(shortMeasure.moves) /
                  static_cast<double>(shortMeasure.people);
    std::printf("\n=== %s: pop %d, %d days ===\n", shortLeg.name,
                shortLeg.population, shortLeg.days);
    std::printf("  %zu people, %zu moves => %.4f moves/person\n",
                shortMeasure.people, shortMeasure.moves, shortPerPerson);
    check(shortMeasure.structuralFailures == 0,
          "leg-60d: the 60-day schedule must tile too (" +
              std::to_string(shortMeasure.structuralFailures) + " violations)");
    check(shortPerPerson <= kMaxShortWindowMovesPerPerson,
          "leg-60d: almost nobody should move in 60 days (moves/person " +
              std::to_string(shortPerPerson) + ", ceiling " +
              std::to_string(kMaxShortWindowMovesPerPerson) +
              "). A high value means the annual rate is being applied without "
              "its duration — the arithmetic slip burst-rate-2026-07 made");
  } catch (const std::exception &e) {
    std::fprintf(stderr, "FAIL: exception: %s\n", e.what());
    return 2;
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "\n%d check(s) failed.\n", g_failures);
    return 1;
  }
  std::printf("\nAll relocation checks passed.\n");
  return 0;
}
