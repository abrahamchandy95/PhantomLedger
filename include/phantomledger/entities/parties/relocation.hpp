#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

/*
  Home area moves. WORLD STATE, not an exporter derivation — the generator
  scores merchant selection as
  `popularity * exp(-distanceMiles / scaleMiles(home))` and the fraud rail's
  geographic axis reads the same home. An exported relocation history the fold
  did not select against would make the corpus DISAGREE WITH ITSELF while
  looking correct, which is worse than immobility. Every consumer of
  `homeAreas` moves together or not at all.

  SOURCE: Census CPS ASEC annual mover rate, 15.9% (1998-99) declining to 8.4%
  (2021). The hazard is ERA-VARYING off that series — `moverRateFor(year)`
  interpolates the two published anchors and clamps outside them. A flat rate
  would put 1990s levels into the 2010s, which the macro layer contradicts.

  A HOUSEHOLD MOVES AS A UNIT, and that is a requirement, not a
  simplification. Coresidents share a home area BY CONSTRUCTION — what the
  `{"home-geo", <household>}` lane buys — so relocating per PERSON would
  silently split households across areas and falsify it. The schedule is keyed
  on the household and expanded to its members, which is why `householdsFor`
  is SHARED with `synth::pii` rather than reproduced here: two derivations of
  the same seed is the kind of duplication that drifts.

  DESTINATIONS FOLLOW THE CPS COMPOSITION: 53.5% within-county, 24.3%
  same-state-different-county, 17.3% different-state, 4.9% from abroad.
  Within-county is not a modelled level (the catalogue has areas and states,
  not counties), so the first two collapse into ONE same-state class — 77.8%
  against 17.3%, normalised over the 95.1% domestic total to 81.8% / 18.2%.

  AREA-CHANGING SHARE. The CPS rate counts anyone who moved, including moves
  within one postal area — different street, same ZCTA — which are invisible
  at this granularity. The mover rate is therefore an UPPER BOUND on the
  area-change rate and `kAreaChangingShare` converts between them.
  CLASS S UNCITED: the direction is arithmetic, the magnitude is declared.

  DETERMINISM: construction runs on its OWN `RngFactory` lane keyed by
  household, exactly as home placement does, so it spends nothing on the
  shared entity stream and cannot move a downstream entity value. Corpus
  movement comes only from the fold reading a home that CHANGES, never from
  building the schedule.

  REGISTERED LIMITATIONS, declared rather than papered over:
    * Household composition is static, so nobody ever moves OUT of a
      household. A young adult leaving home is a real and common move this
      does not produce.
    * FOREIGN MOVES ARE OUT OF SCOPE. The 4.9% from-abroad share is
      IN-migration, and a party's `country` drives locale, PII format and the
      whole identity layer, so a cross-country move would need all of that to
      move too. A relocation stays inside the origin's country.
    * NO AGE OR TENURE TILT, deliberately. Mover rates fall steeply with age
      and owners move far less than renters, both robust findings — but this
      schedule is keyed on the HOUSEHOLD and a household has no single age.
      The eldest member's would encode a householder concept the roster does
      not carry.
 */

namespace PhantomLedger::entity::parties::relocation {

namespace geo = ::PhantomLedger::entity::geography;

// ------------------------------------------------------------ constants

// Census CPS ASEC annual mover rate. Two published anchors; the series is
// close to linear between them.
inline constexpr int kMoverRateEarlyYear = 1998;
inline constexpr double kMoverRateEarly = 0.159;
inline constexpr int kMoverRateLateYear = 2021;
inline constexpr double kMoverRateLate = 0.084;

// Share of moves that change the POSTAL AREA. CLASS S UNCITED — see the
// header note. A same-area move is invisible to every consumer here.
inline constexpr double kAreaChangingShare = 0.85;

// Of domestic moves, the share staying inside the origin's state.
// CPS composition, normalised over the domestic total.
inline constexpr double kSameStateShare = 0.818;

struct Rules {
  double areaChangingShare = kAreaChangingShare;
  double sameStateShare = kSameStateShare;
};

/* The annual probability that a household moves, for a calendar year.
 * Clamped flat outside the two published anchors: extrapolating a declining
 * linear series reaches zero in the 2040s and negative after, and the corpus
 * era runs past 2021. */
[[nodiscard]] inline double moverRateFor(int year) noexcept {
  if (year <= kMoverRateEarlyYear) {
    return kMoverRateEarly;
  }
  if (year >= kMoverRateLateYear) {
    return kMoverRateLate;
  }
  const auto span =
      static_cast<double>(kMoverRateLateYear - kMoverRateEarlyYear);
  const auto t = static_cast<double>(year - kMoverRateEarlyYear) / span;
  return kMoverRateEarly + t * (kMoverRateLate - kMoverRateEarly);
}

// ------------------------------------------------------------ schedule

struct Tenure {
  std::int64_t fromEpoch = 0;
  geo::GeoAreaId area = geo::invalidGeoArea;
};

/* Per-person home-area history, flat-CSR over PersonId-1.
 *
 * INVARIANTS, and each one is load-bearing somewhere:
 *   * every person has at least ONE tenure, and its `fromEpoch` is the
 *     window start — so `areaAt` before any move returns the SAME area
 *     `homeAreas` carries, byte for byte. That is what keeps a
 *     zero-relocation window identical to the pre-round corpus.
 *   * tenures are strictly ascending in `fromEpoch`, so `areaAt` is a
 *     single upper-bound walk and the exporter can emit them in order.
 *   * consecutive tenures have DIFFERENT areas. A move that lands back on
 *     the origin area is not a move at this granularity and is dropped at
 *     construction, never exported as a no-op row. */
class Schedule {
public:
  Schedule() = default;

  Schedule(std::vector<std::uint32_t> offsets, std::vector<Tenure> tenures)
      : offsets_(std::move(offsets)), tenures_(std::move(tenures)) {}

  [[nodiscard]] bool empty() const noexcept { return offsets_.size() < 2; }

  [[nodiscard]] std::size_t personCount() const noexcept {
    return offsets_.empty() ? 0 : offsets_.size() - 1;
  }

  [[nodiscard]] std::span<const Tenure>
  tenures(PersonId person) const noexcept {
    if (!valid(person)) {
      return {};
    }
    const auto i = static_cast<std::size_t>(person) - 1;
    if (i + 1 >= offsets_.size()) {
      return {};
    }
    const auto begin = offsets_[i];
    const auto end = offsets_[i + 1];
    return std::span<const Tenure>{tenures_.data() + begin, end - begin};
  }

  /* The home area occupied at `ts`. `invalidGeoArea` when the person has no
   * schedule — callers treat that exactly as they treat an unbound
   * `homeAreas` carrier, i.e. "no local anchor". */
  [[nodiscard]] geo::GeoAreaId areaAt(PersonId person,
                                      std::int64_t ts) const noexcept {
    const auto rows = tenures(person);
    if (rows.empty()) {
      return geo::invalidGeoArea;
    }
    // Ascending by construction; find the last tenure starting at or before
    // ts. A ts before the first (which is the window start) resolves to the
    // first, so an out-of-window query cannot report "no home".
    std::size_t lo = 0;
    for (std::size_t i = 1; i < rows.size(); ++i) {
      if (rows[i].fromEpoch <= ts) {
        lo = i;
      } else {
        break;
      }
    }
    return rows[lo].area;
  }

  /* Distinct areas anyone ever occupies. The merchant geo-pool builder needs
   * this UNION, not the window-start snapshot: a mover arriving in an area
   * with no pool would silently fall back to the national CDF and the
   * distance-decay model would quietly switch off for them. */
  [[nodiscard]] std::vector<geo::GeoAreaId> allAreas() const {
    std::vector<geo::GeoAreaId> out;
    out.reserve(tenures_.size());
    for (const auto &tenure : tenures_) {
      out.push_back(tenure.area);
    }
    std::ranges::sort(out);
    out.erase(std::ranges::unique(out).begin(), out.end());
    return out;
  }

  /* Home area for every person at `ts`, PersonId-1 indexed — the shape the
   * spending market's population View holds. */
  [[nodiscard]] std::vector<geo::GeoAreaId> snapshotAt(std::int64_t ts) const {
    std::vector<geo::GeoAreaId> out;
    out.reserve(personCount());
    for (std::size_t i = 0; i < personCount(); ++i) {
      out.push_back(areaAt(static_cast<PersonId>(i + 1), ts));
    }
    return out;
  }

  [[nodiscard]] std::size_t moveCount() const noexcept {
    // One tenure per person is the no-move case.
    return tenures_.size() - personCount();
  }

private:
  std::vector<std::uint32_t> offsets_;
  std::vector<Tenure> tenures_;
};

} // namespace PhantomLedger::entity::parties::relocation
