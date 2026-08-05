#pragma once
//
// phantomledger/entities/parties/relocation.hpp
//
// HOME AREA MOVES (relocation-2026-07). World state, not an exporter
// derivation.
//
// THE DEFECT THIS CLOSES. `pii::Address::geoArea` was assigned once on the
// isolated `{"home-geo", <household>}` lane and then fixed for the run, so a
// 20-year corpus contained ZERO relocation. Census CPS ASEC puts the annual
// mover rate at 15.9% (1998-99) declining to 8.4% (2021) — over the owner's
// target window that is most of the population moving at least once, and the
// generator had none of it. It is the natural completion of
// `party-geography-2026-07`: the cardholder-to-merchant distance feature that
// round shipped was measuring against a home that could not move.
//
// WHY THIS IS WORLD STATE AND NOT A LATE PATCH. The generator has scored
// merchant selection as `popularity * exp(-distanceMiles / scaleMiles(home))`
// since `geo-causal-v1`, and the fraud rail's geographic axis reads the same
// home. If the exporter published a relocation history the fold did not
// select against, the corpus would DISAGREE WITH ITSELF — and it would look
// correct, which is worse than the immobility. Every consumer of `homeAreas`
// therefore moves in this round or the round does not ship.
//
// ============================================================ THE MODEL
//
// A HOUSEHOLD MOVES AS A UNIT, and that is a requirement rather than a
// simplification. Coresidents share a home area BY CONSTRUCTION — that is
// what the `{"home-geo", <household>}` lane buys, and
// `party-geography-2026-07` records it as a binding property. Relocating
// per PERSON would silently split households across areas and falsify it.
// The schedule is therefore keyed on the household and expanded to its
// members, which is why `householdsFor` is shared with `synth::pii` rather
// than reproduced here: TWO derivations of the same seed is exactly the kind
// of duplication that drifts.
//
// CONSEQUENCE, DECLARED: household composition is static in this model, so
// nobody ever moves OUT of a household. A young adult leaving home is a real
// and common move that this does not produce. Registered limitation.
//
// THE HAZARD IS ERA-VARYING, off the CPS series. `moverRateFor(year)`
// interpolates the two published anchors and clamps outside them. Mobility
// has declined for four decades and a flat rate would put 1990s levels into
// the 2010s, which the macro layer would then contradict.
//
// DESTINATIONS FOLLOW THE CPS COMPOSITION: 53.5% within-county, 24.3%
// same-state-different-county, 17.3% different-state, 4.9% from abroad.
// Within-county is not a modelled level here — the catalogue has areas and
// states, not counties — so the first two collapse into ONE same-state class,
// giving 77.8% same-state against 17.3% different-state; normalised over the
// 95.1% domestic total that is 81.8% / 18.2%.
//
// FOREIGN MOVES ARE OUT OF SCOPE. The 4.9% from-abroad share is IN-migration,
// and a party's `country` drives locale, PII format and the whole identity
// layer — moving someone between countries mid-run would need all of that to
// move with them. A relocation stays inside the origin's country. Registered
// limitation.
//
// AREA-CHANGING SHARE. The CPS mover rate counts anyone who moved, including
// moves within one postal area — a different street, same ZCTA. Those are
// invisible at this model's granularity, so the mover rate is an UPPER BOUND
// on the area-change rate and `kAreaChangingShare` converts between them.
// CLASS S UNCITED: the direction is arithmetic, the magnitude is declared.
//
// NO AGE OR TENURE TILT, DELIBERATELY. Mover rates fall steeply with age and
// owners move far less than renters, both robust findings — but this schedule
// is keyed on the HOUSEHOLD and a household has no single age. Picking one
// member's would be arbitrary, and picking the eldest would encode a
// householder concept the roster does not carry. Registered as a depth item
// rather than invented.
//
// ============================================================ DETERMINISM
//
// Construction runs on its OWN `RngFactory` lane, keyed by household, exactly
// as home placement does. It therefore spends nothing on the shared entity
// stream and cannot move a downstream entity value — the failure mode that
// cost `merchant-churn-2026-07` three unrelated red gates. Corpus movement in
// this round comes only from the fold reading a home that CHANGES, never from
// building the schedule.

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

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

/// The annual probability that a household moves, for a calendar year.
/// Clamped flat outside the two published anchors: extrapolating a declining
/// linear series reaches zero in the 2040s and negative after, and the corpus
/// era runs past 2021.
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

/// Per-person home-area history, flat-CSR over PersonId-1.
///
/// INVARIANTS, and each one is load-bearing somewhere:
///   * every person has at least ONE tenure, and its `fromEpoch` is the
///     window start — so `areaAt` before any move returns the SAME area
///     `homeAreas` carries, byte for byte. That is what keeps a
///     zero-relocation window identical to the pre-round corpus.
///   * tenures are strictly ascending in `fromEpoch`, so `areaAt` is a
///     single upper-bound walk and the exporter can emit them in order.
///   * consecutive tenures have DIFFERENT areas. A move that lands back on
///     the origin area is not a move at this granularity and is dropped at
///     construction, never exported as a no-op row.
class Schedule {
public:
  Schedule() = default;

  Schedule(std::vector<std::uint32_t> offsets, std::vector<Tenure> tenures)
      : offsets_(std::move(offsets)), tenures_(std::move(tenures)) {}

  [[nodiscard]] bool empty() const noexcept { return offsets_.size() < 2; }

  [[nodiscard]] std::size_t personCount() const noexcept {
    return offsets_.empty() ? 0 : offsets_.size() - 1;
  }

  [[nodiscard]] std::span<const Tenure> tenures(PersonId person) const noexcept {
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

  /// The home area occupied at `ts`. `invalidGeoArea` when the person has no
  /// schedule — callers treat that exactly as they treat an unbound
  /// `homeAreas` carrier, i.e. "no local anchor".
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

  /// Distinct areas anyone ever occupies. The merchant geo-pool builder needs
  /// this UNION, not the window-start snapshot: a mover arriving in an area
  /// with no pool would silently fall back to the national CDF and the
  /// distance-decay model would quietly switch off for them.
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

  /// Home area for every person at `ts`, PersonId-1 indexed — the shape the
  /// spending market's population View holds.
  [[nodiscard]] std::vector<geo::GeoAreaId>
  snapshotAt(std::int64_t ts) const {
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
