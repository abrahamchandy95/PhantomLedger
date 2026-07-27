#pragma once
//
// phantomledger/transfers/fraud/susceptibility.hpp
//
// victimization-v3 (contract docs/card_fraud_victimization.md D2): THE
// SCAM-RAIL HAZARD — who a victim-AUTHORIZED impostor scam reaches, and
// how much they lose.
//
// WHY THIS IS NOT exposure.hpp. V2 tilted victim selection by CARD
// EXPOSURE, and that tilt is right for the unauthorized rails: stolen
// credentials find you through your card and account activity. It is the
// WRONG axis for a scam. An impostor scam arrives by phone, text or
// email; it does not need you to have transacted anywhere. Applying
// exposure to it would say the people who shop least are hardest to
// defraud, which inverts the actual finding. V2 shipped ONE picker for
// all rails with a flat floor precisely so that nobody became
// unreachable — that floor was scam-rail reasoning leaking into the card
// rail, and splitting the two is what this file exists for.
//
// THE TWO AXES ARE OPPOSITE, AND BOTH ARE REAL:
//
//   INCIDENCE falls with age. FTC Consumer Sentinel: younger adults
//   report losing money to fraud MORE OFTEN than older adults — per
//   capita reports peak in the 20s-30s and decline steadily after 60.
//   Younger adults transact with strangers online far more, and that is
//   where the contact happens.
//
//   SEVERITY rises with age, steeply. In the same series, median
//   reported loss climbs monotonically across age bands — the oldest
//   band loses roughly 3x the youngest per incident. Older victims hold
//   larger liquid balances, are targeted with higher-stakes stories
//   (grandchild-in-jail, tech support, government impostor), and the
//   coached sessions run longer.
//
// A model that carried only one of these would be wrong in a way the
// corpus cannot recover from: tilt incidence toward the old and every
// per-capita figure inverts; ignore severity and the amount distribution
// stops distinguishing a student from a retiree. Both directions are
// anchored; the MAGNITUDES below are declared CHOICEs.
//
// WHY THE HAZARD IS EVALUATED AT THE CASE DATE. Persona and age are not
// person constants — they are the whole point of the H2/H3 timeline
// work. Over a 5,000-person 30-year corpus a salaried 45-year-old
// becomes a 75-year-old retiree, and the honest statement is that their
// scam hazard FALLS while their expected loss RISES as that happens.
// Evaluating at window start would have flattened a life course into a
// single label, so `scamWeights` takes the date and the caller rebuilds
// per case. The build consumes no randomness, so it cannot move a
// stream; it is O(population) per case, which is nothing next to the
// fold.
//
// PERSONA CARRIES ONLY NON-AGE STRUCTURE. Persona and age are strongly
// correlated in this model (a retiree is old by construction), so giving
// retirees a low persona factor AND a low age factor would double-count
// one gradient. Every persona factor here is 1.00 unless there is a
// mechanism that is NOT age: a freelancer invoices strangers, a
// small-business owner is the target of vendor/BEC impostors, a student
// is new to payment norms and to job/scholarship offers. The age
// gradient is stated once, in scamAgeIncidence.
//
// THE TILT IS BOUNDED, for the same two reasons exposure.hpp is:
// nobody becomes unreachable (a scam can reach anyone — that is what
// makes the exogenous-attacker model realistic), and a tilt strong
// enough to make persona or age PREDICT the label would recreate this
// arc's original shortcut defect in a new place.
// tests/test_card_victim_baselines.cpp and
// tests/test_card_scam_rail.cpp are the gates.
//
// MEMBERSHIP is answered here too, because it is the same carriers.
// A person who has not joined the bank yet has no account to push money
// out of, so no rail may victimize them before their join date. Death is
// rail-dependent BY DECLARATION: a dead person cannot be talked into
// wiring money, so the scam rails require alive; the unauthorized card
// and ATO rails do NOT, which preserves the existing declared position
// that deceased-account fraud is a real typology (injector.cpp,
// authority U-8 addendum) rather than silently reversing it.
//

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/personas/lifespan.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace PhantomLedger::transfers::fraud::susceptibility {

// Share of the scam hazard that responds to persona x age; the
// remainder is flat (anyone can be called). CHOICE — slightly gentler
// than the exposure tilt because the contact side of a scam is much
// closer to random than card exposure is.
inline constexpr double kScamTiltShare = 0.65;

// Weight clamp, in units of the population mean. The structural half of
// the anti-shortcut condition. CHOICE.
inline constexpr double kMinScamWeight = 0.25;
inline constexpr double kMaxScamWeight = 3.00;

/// NON-AGE persona structure only (see the header note on
/// double-counting). Every value is a declared CHOICE with a named
/// mechanism; the age gradient lives in scamAgeIncidence.
[[nodiscard]] constexpr double
scamPersonaFactor(::PhantomLedger::personas::Type type) noexcept {
  switch (type) {
  case ::PhantomLedger::personas::Type::student:
    // New to payment norms; job, scholarship and rental-deposit
    // offers are pitched at this cohort.
    return 1.10;
  case ::PhantomLedger::personas::Type::freelancer:
    // Invoices and gets paid by strangers as a matter of routine, so a
    // client-impostor story is in-band rather than surprising.
    return 1.15;
  case ::PhantomLedger::personas::Type::smallBusiness:
    // The vendor/BEC impostor target: an urgent payment instruction
    // that looks like a supplier is exactly this persona's normal day.
    return 1.25;
  case ::PhantomLedger::personas::Type::retiree:
    // 1.00 DELIBERATELY. Retirees are the most-cited scam cohort, but
    // in this model that entire effect is age — asserting it twice
    // would manufacture a gradient the research does not support.
    return 1.00;
  case ::PhantomLedger::personas::Type::highNetWorth:
    // Incidence near the reference; the wealth effect belongs to
    // severity, not to how often the phone rings.
    return 1.00;
  case ::PhantomLedger::personas::Type::salaried:
    return 1.00;
  }
  return 1.00;
}

/// PER-CAPITA scam incidence by age, relative to the population mean.
/// DIRECTION anchored (FTC CSN: per-capita reports peak in the 20s-30s
/// and decline after 60); the band values are CHOICEs.
[[nodiscard]] constexpr double scamAgeIncidence(double ageYears) noexcept {
  if (ageYears < 30.0) {
    return 1.35;
  }
  if (ageYears < 40.0) {
    return 1.30;
  }
  if (ageYears < 50.0) {
    return 1.15;
  }
  if (ageYears < 60.0) {
    return 0.95;
  }
  if (ageYears < 70.0) {
    return 0.75;
  }
  if (ageYears < 80.0) {
    return 0.60;
  }
  return 0.50;
}

/// PER-CASE loss multiplier by age. DIRECTION anchored (FTC CSN median
/// reported loss rises monotonically with age, oldest band roughly 3x
/// the youngest); the band values are CHOICEs, and the 0.70 -> 2.20
/// span reproduces that ~3x ratio.
[[nodiscard]] constexpr double scamAgeSeverity(double ageYears) noexcept {
  if (ageYears < 30.0) {
    return 0.70;
  }
  if (ageYears < 40.0) {
    return 0.80;
  }
  if (ageYears < 50.0) {
    return 0.90;
  }
  if (ageYears < 60.0) {
    return 1.00;
  }
  if (ageYears < 70.0) {
    return 1.30;
  }
  if (ageYears < 80.0) {
    return 1.70;
  }
  return 2.20;
}

/// The victim-side view of the population: membership, persona-at-date,
/// age-at-date, and the scam hazard built from them. Borrows the
/// personas pack (which outlives the fold, like the home-area carrier)
/// and holds no randomness — both engines construct it from the same
/// pointer, so it cannot differ between them.
///
/// An ABSENT or short pack makes every predicate permissive and every
/// weight vector empty, which degrades each rail to its pre-v3 draw
/// instead of to something undefined.
class VictimPopulation {
public:
  VictimPopulation() = default;

  VictimPopulation(const ::PhantomLedger::synth::personas::Pack *pack,
                   time::TimePoint windowStart) noexcept
      : pack_(pack), windowStart_(windowStart) {}

  [[nodiscard]] bool ready() const noexcept {
    return pack_ != nullptr && !pack_->timelines.empty() &&
           pack_->birthDates.size() == pack_->timelines.size();
  }

  [[nodiscard]] std::size_t size() const noexcept {
    return ready() ? pack_->timelines.size() : 0;
  }

  /// A customer of the bank at `at`: joined, and (when the caller's rail
  /// requires it) still alive. Permissive when the carriers are absent.
  [[nodiscard]] bool member(std::size_t idx, time::TimePoint at,
                            bool requireAlive) const noexcept {
    if (!ready() || idx >= pack_->timelines.size()) {
      return true;
    }
    if (idx < pack_->joinDays.size()) {
      const auto joined = time::addDays(
          windowStart_, static_cast<int>(pack_->joinDays[idx]));
      if (at < joined) {
        return false;
      }
    }
    if (requireAlive &&
        !::PhantomLedger::synth::personas::timeline::aliveAt(
            pack_->timelines[idx], at)) {
      return false;
    }
    return true;
  }

  /// Age in years at `at`. Deliberately the SAME arithmetic the
  /// mortality walk uses — one age axis for the whole corpus, no second
  /// convention to drift.
  [[nodiscard]] double ageYears(std::size_t idx,
                                time::TimePoint at) const noexcept {
    if (!ready() || idx >= pack_->birthDates.size()) {
      return 0.0;
    }
    return std::max(0.0,
                    ::PhantomLedger::synth::personas::lifespan::detail::
                        ageYearsAt(pack_->birthDates[idx], at));
  }

  /// The per-case loss multiplier for this victim at this date.
  /// 1.0 when the carriers are absent (no severity grading, not zero
  /// amounts).
  [[nodiscard]] double severity(std::size_t idx,
                                time::TimePoint at) const noexcept {
    if (!ready() || idx >= pack_->birthDates.size()) {
      return 1.0;
    }
    return scamAgeSeverity(ageYears(idx, at));
  }

  /// Raw (un-normalized) scam hazard: persona structure x age
  /// incidence, or 0 for anyone who is not an eligible victim at `at`.
  [[nodiscard]] double rawHazard(std::size_t idx,
                                 time::TimePoint at) const noexcept {
    if (!ready() || idx >= pack_->timelines.size()) {
      return 0.0;
    }
    if (!member(idx, at, /*requireAlive=*/true)) {
      return 0.0;
    }
    const auto persona =
        ::PhantomLedger::synth::personas::timeline::personaAt(
            pack_->timelines[idx], at);
    return scamPersonaFactor(persona) * scamAgeIncidence(ageYears(idx, at));
  }

  /// Per-person scam weights over [0, limit), PersonId-1 indexed, mean
  /// ~1.0 across ELIGIBLE victims; ineligible people weigh exactly 0 so
  /// a weighted draw cannot reach them. Normalized against the eligible
  /// mean, blended with the flat floor, then clamped — the same bounded
  /// construction exposure.hpp uses.
  ///
  /// Returns EMPTY when the carriers are absent, which the caller reads
  /// as "stay uniform".
  [[nodiscard]] std::vector<double> scamWeights(time::TimePoint at,
                                                std::size_t limit) const {
    std::vector<double> out;
    if (!ready() || limit == 0) {
      return out;
    }
    out.reserve(limit);

    double total = 0.0;
    std::size_t eligible = 0;
    for (std::size_t idx = 0; idx < limit; ++idx) {
      const double raw = rawHazard(idx, at);
      out.push_back(raw);
      if (raw > 0.0) {
        total += raw;
        ++eligible;
      }
    }

    if (eligible == 0 || !(total > 0.0)) {
      // Nobody eligible (a date before every join, or after every
      // death): an empty vector stands the tilt down rather than
      // inventing a uniform population that is not there.
      out.clear();
      return out;
    }

    const double mean = total / static_cast<double>(eligible);
    for (auto &w : out) {
      if (!(w > 0.0)) {
        w = 0.0;
        continue;
      }
      const double normalized = w / mean;
      w = std::clamp((1.0 - kScamTiltShare) + kScamTiltShare * normalized,
                     kMinScamWeight, kMaxScamWeight);
    }
    return out;
  }

private:
  const ::PhantomLedger::synth::personas::Pack *pack_ = nullptr;
  time::TimePoint windowStart_{};
};

} // namespace PhantomLedger::transfers::fraud::susceptibility
