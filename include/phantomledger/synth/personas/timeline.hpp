#pragma once
//
// phantomledger/synth/personas/timeline.hpp
//
// macro-history-v1 H2 step 1: the persona TIMELINE primitive — the
// single source of persona-AT-DATE truth. The seed assignment is the
// state at THE PERSON'S ANCHOR (sim start for the seed roster; the
// JOIN DATE for the H3 join cohort); the timeline carries the
// transition dates:
//
//   student       -> working (salaried .85 / freelancer .15) at a
//                    work-start age drawn over 19-28, then retiree at
//                    the claiming date
//   salaried,
//   freelancer    -> retiree at an SSA-claiming-shaped date
//   smallBusiness -> working (salaried .70 / freelancer .30) when the
//                    business ends (median-5yr memoryless residual),
//                    then retiree; retirement dominates (a business
//                    that survives to the claiming date closes there)
//   retiree       -> retiree (claim date backdated to <= the anchor)
//   highNetWorth  -> NO transitions at H2 (declared exemption)
//
// Transition dates anchor to the person's BIRTH DATE (durability
// criterion: life events, not window events). The anchor enters ONLY
// through the seed-consistency clamps — the seed assignment is by
// definition the state at the anchor, so
//
//   personaAt(timeline, anchor) == seed type      (pinned invariant)
//
// and the clamps bind ONLY when a drawn date already lies in the
// past: a seed student past the drawn work-start age finishes in
// 90-540 days; a seed worker past the drawn claiming date works
// another 180-1825 days; a seed retiree's claim clamps to the anchor.
// Drawn dates that land in the future stand exactly as drawn.
//
// STATUS: WIRED at H2 steps 2b/2c — `Pack::timelines` carries
// deriveAll's output; salary selection/spans, SSA recipient
// selection/onset, revenue month gating, the AML end-of-window
// persona and the retirement spending step read it (contract:
// docs/h2_persona_timeline.md; authority: docs/fraud_model_audit.md
// U-7 + addendum).
//
// H3 wiring (macro-history-v1, contract docs/h3_mortality_estate.md):
// the timeline ALSO carries the person's DEATH — filled by
// lifespan::derive on the SEPARATE isolated {"mortality", personId}
// lane (three draws; see lifespan.hpp), so every consumer that
// already holds a timeline reads `death` with no new threading.
// Income, spending, rent, cash and family flows stop at death;
// estates and funerals trigger on it; account closure follows it
// (authority U-8 + addendum). The lifespan shares the person's
// anchor, so a joiner is alive at JOIN and dies strictly after it.
//
// DRAW DISCIPLINE: exactly eight draws per person on the
// {"persona-era", personId} lane, unconditional and in a fixed
// documented order (the H3 death fields ride the separate
// {"mortality"} lane and cannot move them) — entity N can never move
// entity N+1, and adding a ninth persona-era draw later is an
// explicit model change.
//

#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/personas/lifespan.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace PhantomLedger::synth::personas::timeline {

using PersonaType = ::PhantomLedger::personas::Type;

// --- Statutory schedule (MEASUREMENT) ----------------------------

// Social Security full retirement age in MONTHS by birth year, per
// the 1983 Amendments schedule (ssa.gov retirement-age chart):
// 65 for cohorts through 1937, +2 months per birth year 1938-1942,
// 66 for 1943-1954, +2 months per birth year 1955-1959, 67 from 1960.
// (SSA's "born January 1 -> previous year" quirk is a declared
// simplification away.)
[[nodiscard]] constexpr int fraMonths(int birthYear) noexcept {
  if (birthYear <= 1937) {
    return 65 * 12;
  }
  if (birthYear <= 1942) {
    return 65 * 12 + 2 * (birthYear - 1937);
  }
  if (birthYear <= 1954) {
    return 66 * 12;
  }
  if (birthYear <= 1959) {
    return 66 * 12 + 2 * (birthYear - 1954);
  }
  return 67 * 12;
}

// --- Distribution shapes (authority U-7) -------------------------

// Claiming-age mixture (CHOICE, anchored to the SSA Annual
// Statistical Supplement claiming-age tables; one distribution
// era-wide — per-cohort claiming shares are a registered upgrade,
// like the single mortality table year):
//   .30 at 62 exactly, .10 uniform over [63y, FRA), .45 at FRA,
//   .05 uniform over (FRA, 70y), .10 at 70 exactly.
// The FRA itself is cohort-varying via fraMonths, so era variation
// enters through the statutory schedule.
[[nodiscard]] inline int claimAgeMonths(double u, int fra) noexcept {
  constexpr int k62 = 62 * 12;
  constexpr int k63 = 63 * 12;
  constexpr int k70 = 70 * 12;

  if (u < 0.30) {
    return k62;
  }
  if (u < 0.40) {
    const double f = (u - 0.30) / 0.10;
    const int span = fra - k63; // >= 24 months for every statutory FRA
    return k63 + std::min(span - 1, static_cast<int>(f * span));
  }
  if (u < 0.85) {
    return fra;
  }
  if (u < 0.90) {
    const double f = (u - 0.85) / 0.05;
    const int span = k70 - fra - 1; // months strictly between FRA and 70
    return fra + 1 + std::min(span - 1, static_cast<int>(f * span));
  }
  return k70;
}

// Student work-start age in YEARS (CHOICE: NCES completion ages +
// BLS student-employment profiles — non-completers enter at 19-21,
// bachelor's completion mass at 22-26, graduate tail to 28).
[[nodiscard]] inline int workStartAgeYears(double u) noexcept {
  constexpr int kFirstAge = 19;
  constexpr double kWeights[] = {0.05, 0.05, 0.08, 0.20, 0.20,
                                 0.15, 0.10, 0.07, 0.05, 0.05};
  double acc = 0.0;
  for (int i = 0; i < 10; ++i) {
    acc += kWeights[i];
    if (u < acc) {
      return kFirstAge + i;
    }
  }
  return kFirstAge + 9;
}

// Residual business lifetime in DAYS from a memoryless exponential
// hazard whose median is five years (TYPOLOGY: constant hazard;
// MEASUREMENT anchor: BLS BED ~50% five-year establishment survival).
// Memorylessness makes the anchor-relative start mathematically
// equivalent to any backdated business start. Clamped to
// [30 days, 40 years].
[[nodiscard]] inline int businessResidualDays(double u) noexcept {
  constexpr double kMedianDays = 1826.0; // five years
  constexpr double kLn2 = 0.6931471805599453;
  const double capped = std::min(u, 1.0 - 1e-12);
  const double raw = -std::log1p(-capped) * kMedianDays / kLn2;
  const double clamped = std::clamp(raw, 30.0, 14610.0);
  return static_cast<int>(std::lround(clamped));
}

// --- Date helpers -------------------------------------------------

// The calendar point where a person born on `dob` reaches
// `ageMonths`. Day-of-month clamps to 28 so every month is legal;
// birthday-relative jitter is added by the caller.
[[nodiscard]] inline time::TimePoint atAgeMonths(time::CalendarDate dob,
                                                 int ageMonths) {
  const int total =
      dob.year * 12 + static_cast<int>(dob.month) - 1 + ageMonths;
  time::CalendarDate cd = dob;
  cd.year = total / 12;
  cd.month = static_cast<unsigned>(total % 12 + 1);
  cd.day = std::min(dob.day, 28U);
  return time::makeTime(cd);
}

// --- The timeline -------------------------------------------------

/// Lifecycle transition dates for one person. Dates are meaningful
/// per seed type (see personaAt); all are derived once, deterministic
/// on (worldSeed, personId, seed, dob, anchor).
struct Timeline {
  PersonaType seed = PersonaType::salaried;
  /// The working-life type: the student's destination, the
  /// smallBusiness owner's post-business type, otherwise the seed.
  PersonaType working = PersonaType::salaried;
  /// Student seeds only: study -> work.
  time::TimePoint workStart{};
  /// smallBusiness seeds only: the business closes (== retirement
  /// when the business survives to the claiming date).
  time::TimePoint businessEnd{};
  /// The SSA-claiming-shaped retirement date. For retiree seeds it is
  /// backdated (<= the anchor); for highNetWorth it is a far-future
  /// sentinel that personaAt never consults.
  time::TimePoint retirement{};

  // H3 wiring: the lifespan primitive's outputs, drawn on the
  // SEPARATE {"mortality", personId} lane (lifespan.hpp: three
  // draws; alive-at-anchor invariant — death strictly after the
  // person's anchor; age-120 cap; sex is a latent mortality
  // attribute). Income stops here; personaAt is deliberately
  // death-agnostic — consumers gate on `death` (or aliveAt below)
  // explicitly.
  time::TimePoint death{};
  bool male = false;
};

struct Inputs {
  entity::PersonId person = 0;
  PersonaType seed = PersonaType::salaried;
  time::CalendarDate dob{};
  /// The person's SEED-STATE ANCHOR: sim start for the seed roster,
  /// the JOIN DATE for the H3 join cohort (the field keeps its H2
  /// name; deriveAll fills it per person from Pack::joinDays).
  time::TimePoint simStart{};
};

namespace detail {

inline void requireInputs(const Inputs &in) {
  if (in.person == 0) {
    throw std::invalid_argument("timeline::derive: person must be >= 1");
  }
  if (in.dob.year < 1850 || in.dob.year > 2200) {
    throw std::invalid_argument("timeline::derive: dob.year out of range");
  }
  if (in.dob.month < 1 || in.dob.month > 12) {
    throw std::invalid_argument("timeline::derive: dob.month out of range");
  }
  if (in.dob.day < 1 || in.dob.day > 31) {
    throw std::invalid_argument("timeline::derive: dob.day out of range");
  }
}

// Seed-consistency clamp: a drawn life date that already lies at or
// before the anchor contradicts the seed state, so the event settles
// a drawn number of days past the anchor instead. Future dates stand
// exactly as drawn.
[[nodiscard]] inline time::TimePoint settled(time::TimePoint drawn,
                                             time::TimePoint anchor,
                                             int settleDays) {
  return drawn > anchor ? drawn : time::addDays(anchor, settleDays);
}

} // namespace detail

[[nodiscard]] inline Timeline derive(const random::RngFactory &factory,
                                     const Inputs &in) {
  detail::requireInputs(in);

  auto rng = factory.rng({"persona-era", std::to_string(in.person)});

  // The eight draws, unconditional, in this order (contract):
  const double uClaim = rng.nextDouble();                          // 1
  const int claimJitter = static_cast<int>(rng.uniformInt(0, 61)); // 2
  const double uWorkAge = rng.nextDouble();                        // 3
  const double uWorkDest = rng.nextDouble();                       // 4
  const double uBizResid = rng.nextDouble();                       // 5
  const double uBizDest = rng.nextDouble();                        // 6
  const int settleShort = static_cast<int>(rng.uniformInt(90, 541));  // 7
  const int settleLong = static_cast<int>(rng.uniformInt(180, 1826)); // 8

  const int fra = fraMonths(in.dob.year);
  const auto claimDate =
      time::addDays(atAgeMonths(in.dob, claimAgeMonths(uClaim, fra)),
                    claimJitter);

  Timeline tl;
  tl.seed = in.seed;

  switch (in.seed) {
  case PersonaType::highNetWorth:
    // Declared H2 exemption: no transitions (revisit with the CEX/H3
    // budget work). Sentinel far beyond the mortality table.
    tl.working = PersonaType::highNetWorth;
    tl.retirement = atAgeMonths(in.dob, 150 * 12);
    break;

  case PersonaType::retiree:
    // Seed-consistency: a seed retiree is retired AT the anchor; the
    // drawn claim backdates, clamped so it never lands after it.
    tl.working = PersonaType::retiree;
    tl.retirement = std::min(claimDate, in.simStart);
    break;

  case PersonaType::student: {
    tl.working =
        uWorkDest < 0.85 ? PersonaType::salaried : PersonaType::freelancer;
    const auto drawn =
        atAgeMonths(in.dob, workStartAgeYears(uWorkAge) * 12);
    tl.workStart = detail::settled(drawn, in.simStart, settleShort);
    tl.retirement =
        std::max(claimDate, time::addDays(tl.workStart, 365)); // safety
    break;
  }

  case PersonaType::smallBusiness: {
    tl.working =
        uBizDest < 0.70 ? PersonaType::salaried : PersonaType::freelancer;
    tl.retirement = detail::settled(claimDate, in.simStart, settleLong);
    const auto residualEnd =
        time::addDays(in.simStart, businessResidualDays(uBizResid));
    tl.businessEnd = std::min(residualEnd, tl.retirement);
    break;
  }

  case PersonaType::salaried:
  case PersonaType::freelancer:
    tl.working = in.seed;
    tl.retirement = detail::settled(claimDate, in.simStart, settleLong);
    break;
  }

  // H3: the lifespan, on its OWN isolated lane — the eight draws
  // above are byte-identical with or without it. It shares the
  // person's anchor, so a joiner's death lands strictly after their
  // join date (alive-at-join).
  const auto ls = lifespan::derive(factory, lifespan::Inputs{
                                                .person = in.person,
                                                .dob = in.dob,
                                                .simStart = in.simStart,
                                            });
  tl.death = ls.death;
  tl.male = ls.male;

  return tl;
}

/// The effective persona on a date. Pure; the seed dominates only
/// through the transition dates derived above. Deliberately
/// death-agnostic — gate on aliveAt separately.
[[nodiscard]] inline PersonaType personaAt(const Timeline &tl,
                                           time::TimePoint at) noexcept {
  if (tl.seed == PersonaType::highNetWorth) {
    return PersonaType::highNetWorth;
  }
  if (tl.seed == PersonaType::retiree) {
    return PersonaType::retiree;
  }
  if (at >= tl.retirement) {
    return PersonaType::retiree;
  }
  if (tl.seed == PersonaType::student && at < tl.workStart) {
    return PersonaType::student;
  }
  if (tl.seed == PersonaType::smallBusiness && at < tl.businessEnd) {
    return PersonaType::smallBusiness;
  }
  return tl.working;
}

/// Alive strictly before the death instant (H3).
[[nodiscard]] inline bool aliveAt(const Timeline &tl,
                                  time::TimePoint at) noexcept {
  return at < tl.death;
}

// --- Batch derivation + consumer helpers (H2 step 2b) -------------

/// Every person's timeline from the seed assignment + the single-age
/// carrier (Pack::birthDates), on the same world-seed factory the dob
/// lanes use. Fills Pack::timelines at the entities stage. `joinDays`
/// (Pack::joinDays; optional — empty means everyone anchors at sim
/// start, the pre-3c-ii shape) moves each joiner's seed-state anchor
/// to their join date (H3 part 3c-ii: the age axis and the alive
/// invariant bind at JOIN for the join cohort).
[[nodiscard]] inline std::vector<Timeline>
deriveAll(std::uint64_t worldSeed, time::TimePoint simStart,
          const entity::behavior::Assignment &assignment,
          const std::vector<time::CalendarDate> &birthDates,
          std::span<const std::uint32_t> joinDays = {}) {
  if (birthDates.size() != assignment.byPerson.size()) {
    throw std::invalid_argument(
        "timeline::deriveAll: birthDates must cover the assignment "
        "(fill Pack::birthDates first)");
  }
  const bool anchored = joinDays.size() == assignment.byPerson.size();
  const random::RngFactory factory{worldSeed};
  std::vector<Timeline> out;
  out.reserve(assignment.byPerson.size());
  for (std::size_t idx = 0; idx < assignment.byPerson.size(); ++idx) {
    const auto anchor =
        anchored ? simStart + time::Days{static_cast<int>(joinDays[idx])}
                 : simStart;
    out.push_back(derive(factory, Inputs{
                                      .person = static_cast<entity::PersonId>(
                                          idx + 1),
                                      .seed = assignment.byPerson[idx],
                                      .dob = birthDates[idx],
                                      .simStart = anchor,
                                  }));
  }
  return out;
}

/// When PAYROLL becomes possible for this life: students at the
/// study->work transition, small-business owners after the business
/// closes (they take a job), everyone else from the beginning of
/// time. Payroll is active on [payrollStart(tl), min(tl.retirement,
/// tl.death)) since H3.
[[nodiscard]] inline time::TimePoint
payrollStart(const Timeline &tl) noexcept {
  if (tl.seed == PersonaType::student) {
    return tl.workStart;
  }
  if (tl.seed == PersonaType::smallBusiness) {
    return tl.businessEnd;
  }
  return time::TimePoint{};
}

} // namespace PhantomLedger::synth::personas::timeline
