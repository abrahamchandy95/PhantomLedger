#pragma once
//
// phantomledger/synth/merchants/lifecycle.hpp
//
// WHEN EACH MERCHANT OPENS AND CLOSES.
//
// ===================================================================
// WHY THIS EXISTS (merchant-churn-2026-07)
//
// `makeCatalog` took no window and no date; `merchant::Record` carried no
// time field at all. **Every merchant in the catalogue was live on every
// day of the run**, so across the owner's 20-year target window not one
// merchant ever opened and not one ever closed. At population 6,000 that
// is 490 acceptance endpoints, fixed, for 7,305 days.
//
// This is not a realism rounding error. It deletes an entire axis a fraud
// model expects to be informative — "this merchant is three months old",
// "this merchant closed last year" — and it makes the merchant side of the
// graph structurally stationary while the population around it ages, dies
// and is replaced (macro-history-v1 models all three for PEOPLE).
//
// ===================================================================
// THE SURVIVAL MODEL
//
// A THREE-BAND ANNUAL DEATH HAZARD, each band derived from a published
// BLS Business Employment Dynamics survival point for RETAIL TRADE
// (NAICS 44) rather than from a fitted curve, so every number here traces
// to one source figure:
//
// SOURCE: BLS Business Employment Dynamics, "Table 7. Survival of private
// sector establishments by opening year", NAICS 44 (Retail Trade),
// `bls.gov/bdm/us_age_naics_44_table7.txt`. The MARCH-1994 birth cohort is
// the longest the series tracks: 80,604 establishments at birth, of which
// 46,469 survive to March 1998 and 38,669 to March 2000.
//
//   PUBLISHED POINT                            FITTED HAZARD
//   ------------------------------------------ ---------------------------
//   Mar-1995,  1 year:  82.8%                  h(age < 1)    = 0.1720
//     h = 1 - 0.828
//   Mar-1998,  4 years: 57.7%  (46,469/80,604) h(1 <= age < 5) = 0.1134
//     h = 1 - (0.577 / 0.828)^(1/3)
//   Mar-2025, 31 years: 13.7%                  h(age >= 5)   = 0.0494
//     implied 5-year survival 0.828*(1-0.1134)^4 = 0.5116
//     h = 1 - (0.137 / 0.5116)^(1/26)
//
// THE FOURTH POINT IS THE VALIDATION, AND IT IS NOT FITTED. March 2000
// (6 years) is published at 48.0% (38,669/80,604) and the three hazards
// above were fitted WITHOUT it. They reproduce it at 48.63% — a 0.63pp
// error on a point the calibration never saw. Three constants matching
// three points proves only arithmetic; a fourth point falling out within
// two thirds of a percentage point is evidence that the THREE-BAND
// STRUCTURE is right and not curve-fitting.
//
// CLASS: CITED (accessed 2026-07-30), levels MEASUREMENT-derived.
//
// RETRIEVAL NOTE, because it will come up again: `bls.gov` returns HTTP 403
// to direct programmatic fetches of this file, so the table cannot be
// re-pulled from a build or a test. The figures above were read through a
// search index and cross-checked by their own internal arithmetic — the
// published establishment COUNTS divide to the published PERCENTAGES
// (46,469/80,604 = 57.65%, 38,669/80,604 = 47.97%), which is the check that
// distinguishes a real table row from a plausible-looking number. Anyone
// revising these must reproduce that consistency check.
//
// PRIOR STATE, RECORDED BECAUSE IT IS INSTRUCTIVE: this block previously
// claimed retail 1-year survival of ~84.2% and 5-year of ~58.3%, and the
// hazards were 0.158 / 0.1145 / 0.0540. BOTH CITED FIGURES WERE WRONG for
// NAICS 44 — 84.2% and 58.3% are not the retail cohort's numbers, and 58.3%
// is suspiciously close to the 57.7% FOUR-year figure, which is likely
// where it came from. The stated arithmetic was ALSO wrong: the comment
// asserted `0.842 * (1 - 0.1145)^4 = 0.583` when the left side is 0.5177.
// The two errors partly cancelled, so the old constants still landed within
// 1.5pp of every published point and nothing measured them. **A derivation
// written out in a comment is not a derivation until someone evaluates it.**
//
// INCUMBENTS ARE MATURE, AND THAT IS NOT AN APPROXIMATION. The BLS
// survival curve describes a BIRTH cohort. The merchants a window opens
// with are not a birth cohort — they are the survivors of every earlier
// cohort, so they are survivorship-biased toward maturity and their
// FORWARD hazard is legitimately the mature band. Incumbents therefore
// draw `h = 0.0494`, giving 20-year survival `(1 - 0.0494)^20 = 0.363`.
// Roughly two thirds of the merchants a 20-year window opens with should
// be gone by the end. Applying the birth-cohort curve to incumbents
// instead would have over-killed them and is the error this note exists
// to prevent.
//
// ===================================================================
// RECESSION MODULATION (owner ruling, 2026-07-29)
//
// BLS reports the 2001 and 2008 birth cohorts as the worst-surviving in
// the series, and the repo already carries the NBER recession month count
// per year in `econ::MacroYear::recessionMonths` (macro-history-v1). The
// hazard is scaled by `1 + kRecessionHazardLift * (recessionMonths / 12)`,
// so a year fully inside a contraction carries the full lift and a partial
// year carries its fraction.
//
// This is a MODULATION, not a new series: it consumes a column the
// authority document already classes MEASURED (NBER), so it adds no
// uncited number beyond the lift coefficient itself.
//
// ===================================================================
// THE CATALOGUE MUST GROW, WHICH IS THE OWNER'S ACTUAL COMPLAINT
//
// If deaths are modelled and births are not, the live merchant count
// decays and a 20-year run ends with a THINNER pool than it started —
// the opposite of the fix. Births must replace deaths, so a long window
// needs more records than its live count:
//
//   births over the window  ~  L * h_mature * years
//   total records           ~  L * (1 + h_mature * years)
//
// At 20 years and h = 0.054 that is ~2.1x the live count. `GenerationPlan`
// therefore sizes the catalogue from the WINDOW LENGTH, and the live count
// at any instant stays near the pre-lifecycle total. **A person's reachable
// merchant universe over 20 years roughly doubles, while what they can
// reach on any single DAY is unchanged** — which is the realism the round
// is for, delivered without inflating daily choice.
//
// ===================================================================
// DRAW DISCIPLINE
//
// ISOLATED PER-MERCHANT LANES off `lifeSeed`, exactly as
// `placeGeography` does, so lifecycle assignment appends nothing to the
// shared entity stream. That does NOT make the round corpus-neutral —
// selection now sees a time-varying live set, so transaction destinations
// genuinely move and all four goldens re-pin. The isolation buys
// something narrower and still worth having: the lifecycle draws cannot
// perturb any OTHER entity-stage value, so a change to this file moves
// merchant intervals and nothing else.
//

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/synth/econ/catalog.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>

namespace PhantomLedger::synth::merchants {

namespace geoent = ::PhantomLedger::entity::geography;

// Annual death hazards by establishment age band. See the header note for
// the BLS survival point each one reproduces.
inline constexpr double kHazardFirstYear = 0.1720;
inline constexpr double kHazardYears1To5 = 0.1134;
inline constexpr double kHazardMature = 0.0494;

// The published survival points these reproduce, kept as constants so the
// gate can assert the CALIBRATION rather than only the implementation. A
// check that derives its expectation from the hazard above verifies that the
// mechanism applies the hazard it declares — which is worth having, but it
// passes for ANY hazard. These are what make it a claim about the world.
inline constexpr double kBlsSurvival1Year = 0.828;
inline constexpr double kBlsSurvival4Year = 0.577;
inline constexpr double kBlsSurvival6Year = 0.480; // NOT fitted; validation
inline constexpr double kBlsSurvival31Year = 0.137;

// Fractional hazard increase for a year spent entirely in an NBER
// contraction. CLASS S UNCITED — the DIRECTION is BLS-anchored (the 2001
// and 2008 cohorts survive worst) but the magnitude is a declared choice.
inline constexpr double kRecessionHazardLift = 0.60;

struct LifecycleRules {
  double hazardFirstYear = kHazardFirstYear;
  double hazardYears1To5 = kHazardYears1To5;
  double hazardMature = kHazardMature;
  double recessionHazardLift = kRecessionHazardLift;

  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;
    r.check([&] { v::unit("hazardFirstYear", hazardFirstYear); });
    r.check([&] { v::unit("hazardYears1To5", hazardYears1To5); });
    r.check([&] { v::unit("hazardMature", hazardMature); });
    r.check([&] { v::nonNegative("recessionHazardLift", recessionHazardLift); });
  }
};

namespace detail {

[[nodiscard]] inline std::string_view lifeSerial(std::array<char, 24> &buf,
                                                 std::uint64_t serial) {
  const auto [ptr, ec] = std::to_chars(
      buf.data(), buf.data() + buf.size(),
      static_cast<unsigned long long>(serial));
  (void)ec;
  return std::string_view(buf.data(),
                          static_cast<std::size_t>(ptr - buf.data()));
}

// A replacement's counterparty Key. Keeps the donor's BANK (so the
// `Bank::internal` acquiring share is preserved across the churn population
// exactly as `makeCatalog` set it) while taking a fresh serial, so no
// replacement can collide with an incumbent.
[[nodiscard]] inline entity::Key
makeReplacementKey(const entity::merchant::Record &donor,
                   std::uint64_t serial) {
  return entity::makeKey(donor.counterpartyId.role, donor.counterpartyId.bank,
                         serial);
}

// The hazard for a merchant of `ageYears` during calendar year `year`,
// with the NBER recession modulation applied.
[[nodiscard]] inline double hazardFor(const LifecycleRules &rules,
                                      const econ::MacroSeries *macro,
                                      double ageYears, int year) {
  const double base = ageYears < 1.0    ? rules.hazardFirstYear
                      : ageYears < 5.0  ? rules.hazardYears1To5
                                        : rules.hazardMature;
  if (macro == nullptr || !macro->contains(year)) {
    return std::clamp(base, 0.0, 1.0);
  }
  const double recessionFraction =
      static_cast<double>(macro->at(year).recessionMonths) / 12.0;
  const double lifted =
      base * (1.0 + rules.recessionHazardLift * recessionFraction);
  return std::clamp(lifted, 0.0, 1.0);
}

} // namespace detail

// Append the churn REPLACEMENTS: the merchants born during the window that
// keep the live count from decaying as the base catalogue's incumbents die.
//
// MUST run before `assignLifecycle`, which reads `firstEpoch` to tell a
// replacement from an incumbent, and AFTER `placeGeography`, whose per-record
// lanes are keyed on the label serial and would otherwise not cover these.
// Geography for the new records is assigned here from the same lanes.
//
// ISOLATED LANE, and that is the whole point of this function existing
// rather than sizing `makeCatalog` by the window: `makeCatalog` spends one
// shared-entity-stream draw per core record, so growing it there shifted
// every downstream entity value and produced 51,079 account-closure
// violations in a gate that has nothing to do with merchants. Everything
// below draws off `churnSeed` only.
//
// Each replacement inherits the ECONOMIC SHAPE of the base catalogue — the
// same weight distribution, category mix and footprint mix — because a
// merchant that opens in 2012 is not systematically bigger or smaller than
// one that opened in 2000. Anything else would make merchant age a proxy
// for size, and size drives selection.
inline void appendChurnReplacements(entity::merchant::Catalog &catalog,
                                    const time::Window &window,
                                    std::uint64_t churnSeed,
                                    const GenerationPlan &plan,
                                    const LifecycleRules &rules = {}) {
  if (catalog.records.empty() || window.days <= 0) {
    return;
  }

  const double windowYears = static_cast<double>(window.days) / 365.25;
  const auto baseCount = catalog.records.size();

  // Expected deaths over the window among the incumbents, which is exactly
  // how many births are needed to hold the live count flat.
  const auto replacementCount = static_cast<std::size_t>(
      std::llround(static_cast<double>(baseCount) * rules.hazardMature *
                   windowYears));
  if (replacementCount == 0) {
    return;
  }

  const random::RngFactory factory{churnSeed};

  // Serial space continues past the base catalogue so a replacement can
  // never collide with an incumbent key. Both are Role::merchant.
  std::uint64_t nextSerial = 0;
  for (const auto &record : catalog.records) {
    nextSerial = std::max(nextSerial, record.label.value);
  }
  ++nextSerial;

  // The base weight and footprint mixes, sampled by copying a randomly
  // chosen incumbent's economic attributes. Copying rather than redrawing
  // guarantees the replacement population is distributionally identical to
  // the base one without restating the lognormal parameters here — and so a
  // change to `GenerationPlan` cannot silently desynchronise the two.
  catalog.records.reserve(baseCount + replacementCount);

  for (std::size_t i = 0; i < replacementCount; ++i) {
    std::array<char, 24> buf{};
    const auto serial = detail::lifeSerial(buf, nextSerial + i);
    auto rng = factory.rng({"merchant-churn", serial});

    const auto donorIdx =
        static_cast<std::size_t>(rng.nextDouble() * static_cast<double>(baseCount));
    const auto &donor = catalog.records[std::min(donorIdx, baseCount - 1)];

    // Birth day, uniform over the window. A replacement born on the last
    // day is legal and correct — a business that opens in the final month
    // is exactly the "brand new merchant" case a fraud model should see.
    const auto birthOffset = std::min(
        static_cast<int>(rng.nextDouble() * static_cast<double>(window.days)),
        window.days - 1);
    const auto birthPoint = time::addDays(window.start, birthOffset);

    entity::merchant::Record born{};
    born.label = entity::merchant::Label{nextSerial + i};
    born.counterpartyId = detail::makeReplacementKey(donor, nextSerial + i);
    born.category = donor.category;
    born.weight = donor.weight;
    born.location = donor.location;
    born.footprint = donor.footprint;
    // Ownership is stamped later by `assignMerchantOwners`, which is
    // draw-free and reads the key alone — replacements go through the same
    // predicate as incumbents, so merchant age cannot correlate with
    // whether the bank holds an owner record.
    born.firstEpoch = time::toEpochSeconds(birthPoint);
    born.lastEpochExcl = entity::merchant::kEpochMax;

    catalog.records.push_back(born);
  }
}

// Assign `[firstEpoch, lastEpochExcl)` to every record in `catalog`.
//
// Incumbents open BEFORE the window (their exact prior open date is not
// modelled and nothing reads it, so it is the epoch floor) and are killed
// forward year by year from the window start. Births are placed uniformly
// inside the window and then killed forward from their own birth date
// through the age bands, so a merchant born in year 18 really does face
// the first-year hazard.
//
// A death drawn beyond the window end leaves `lastEpochExcl` at the epoch
// ceiling: the merchant outlives the run, which is the correct
// representation of right-censoring and keeps `liveAt` cheap.
inline void assignLifecycle(entity::merchant::Catalog &catalog,
                            const time::Window &window, std::uint64_t lifeSeed,
                            const econ::MacroSeries *macro = nullptr,
                            const LifecycleRules &rules = {}) {
  if (catalog.records.empty() || window.days <= 0) {
    return;
  }

  const random::RngFactory factory{lifeSeed};

  const auto windowStart = window.start;
  const auto windowEndExcl = time::addDays(windowStart, window.days);

  const auto endEpoch = time::toEpochSeconds(windowEndExcl);

  for (auto &record : catalog.records) {
    std::array<char, 24> buf{};
    const auto serial = detail::lifeSerial(buf, record.label.value);
    auto rng = factory.rng({"merchant-life", serial});

    // COHORT IS STRUCTURAL, NOT DRAWN. A record already carrying a
    // `firstEpoch` is a churn REPLACEMENT that `appendChurnReplacements`
    // placed and dated; everything else is a BASE record, i.e. an
    // incumbent live when the window opened. Deciding this with a coin
    // instead would let the incumbent count drift from the base catalogue
    // size, which is the one quantity the live-count stationarity argument
    // depends on.
    const bool incumbent = !record.lifecycleAssigned();

    // An incumbent's real open date precedes the window and is not
    // modelled (nothing reads it), so it stays the epoch floor while its
    // exposure still begins at the window edge.
    const auto birthPoint =
        incumbent ? windowStart : time::fromEpochSeconds(record.firstEpoch);
    const auto birthEpoch = time::toEpochSeconds(birthPoint);

    if (incumbent) {
      record.firstEpoch = entity::merchant::kEpochMin;
    }
    record.lastEpochExcl = entity::merchant::kEpochMax;

    // Kill walk: one Bernoulli per year of exposure, at the hazard for
    // that year and that age. The death DAY is placed uniformly inside
    // the fatal year — without that, every closure lands on the cohort
    // anniversary and shows up as a spike in any monthly series.
    auto cursor = birthPoint;
    // Incumbents are mature by survivorship (see the header note); births
    // start at age zero and walk up through the bands.
    double ageYears = incumbent ? 5.0 : 0.0;

    while (time::toEpochSeconds(cursor) < endEpoch) {
      const auto cal = time::toCalendarDate(cursor);
      const double hazard = detail::hazardFor(rules, macro, ageYears, cal.year);

      const double dieU = rng.nextDouble();
      const double dayU = rng.nextDouble();

      if (dieU < hazard) {
        const auto dayOffset = std::max(1, static_cast<int>(dayU * 365.0));
        const auto deathEpoch =
            time::toEpochSeconds(time::addDays(cursor, dayOffset));
        if (deathEpoch < endEpoch) {
          // Guarantee a non-empty interval: a birth and a death on the
          // same instant would make the merchant unreachable while still
          // occupying a catalogue slot.
          record.lastEpochExcl = std::max(deathEpoch, birthEpoch + 1);
        }
        break;
      }

      cursor = time::addDays(cursor, 365);
      ageYears += 1.0;
    }
  }
}

} // namespace PhantomLedger::synth::merchants
