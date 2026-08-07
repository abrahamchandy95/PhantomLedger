#pragma once

#include "phantomledger/activity/spending/market/population/census.hpp"
#include "phantomledger/activity/spending/market/population/paydays.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/parties/behaviors.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace PhantomLedger::activity::spending::market::population {

class View {
public:
  View() = default;

  View(std::vector<entity::Key> primary, std::vector<personas::Type> kinds,
       std::vector<entity::behavior::Persona> objects, Paydays paydays,
       std::vector<entity::geography::GeoAreaId> homeAreas = {},
       std::vector<std::uint32_t> retirementDays = {},
       std::vector<std::uint32_t> deathDays = {},
       const entity::parties::relocation::Schedule *relocation = nullptr,
       std::vector<std::uint32_t> depositOffsets = {},
       std::vector<entity::Key> depositAccounts = {})
      : primary_(std::move(primary)), kinds_(std::move(kinds)),
        objects_(std::move(objects)), paydays_(std::move(paydays)),
        homeAreas_(std::move(homeAreas)),
        retirementDays_(std::move(retirementDays)),
        deathDays_(std::move(deathDays)), relocation_(relocation),
        depositOffsets_(std::move(depositOffsets)),
        depositAccounts_(std::move(depositAccounts)) {}

  [[nodiscard]] std::uint32_t count() const noexcept {
    return static_cast<std::uint32_t>(primary_.size());
  }

  [[nodiscard]] entity::Key primary(entity::PersonId p) const noexcept {
    return primary_[index(p)];
  }

  /* Every deposit account this person owns, PRIMARY FIRST. Falls back to a
   * one-entry view of the primary when no ownership carrier is bound, so a
   * harness that builds a Census without one behaves exactly as it did when
   * the primary was the only reachable account. */
  [[nodiscard]] std::span<const entity::Key>
  deposits(entity::PersonId p) const noexcept {
    const auto i = index(p);
    if (i + 1 >= depositOffsets_.size()) {
      return {primary_.data() + i, 1};
    }
    const auto start = depositOffsets_[i];
    const auto end = depositOffsets_[i + 1];
    if (end <= start) {
      return {primary_.data() + i, 1};
    }
    return {depositAccounts_.data() + start, end - start};
  }

  [[nodiscard]] personas::Type kind(entity::PersonId p) const noexcept {
    return kinds_[index(p)];
  }

  [[nodiscard]] const entity::behavior::Persona &
  object(entity::PersonId p) const noexcept {
    return objects_[index(p)];
  }

  [[nodiscard]] bool isPayday(entity::PersonId p,
                              std::uint32_t dayIndex) const noexcept {
    return paydays_.isPayday(index(p), dayIndex);
  }

  [[nodiscard]] const Paydays &paydays() const noexcept { return paydays_; }

  // geo-causal-v1 (G2a): the person's home area for card-present
  // distance-decay selection. invalidGeoArea when no carrier is bound
  // (the monolith reference oracle) — the caller treats that as "no
  // local anchor" and falls back to the national selection.
  //
  // relocation-2026-07: this is the home as of the LAST `refreshHomes` — i.e.
  // the current month, not the window start. `payments.cpp` reads it here
  // rather than off the cached `Spender` copy, because `prepareSpenders` runs
  // ONCE for the whole fold and cannot be re-run.
  // merchant-selection-2026-08: the whole vector, index-aligned to personIdx,
  // for the monthly favourite-membership pass. Reflects the CURRENT homes —
  // `refreshHomes` runs before `evolveAll`, which is what lets a household that
  // moved this month acquire favourites near its NEW home.
  [[nodiscard]] std::span<const entity::geography::GeoAreaId>
  homeAreas() const noexcept {
    return homeAreas_;
  }

  [[nodiscard]] entity::geography::GeoAreaId
  homeArea(entity::PersonId p) const noexcept {
    const auto i = index(p);
    return i < homeAreas_.size() ? homeAreas_[i]
                                 : entity::geography::invalidGeoArea;
  }

  // relocation-2026-07: re-point every person's home at `ts` from the
  // schedule. Called from the monthly commerce evolver — the same hook
  // merchant liveness uses, and for the same reason: it is the only
  // per-instant callback the fold has.
  //
  // NO-OP without a bound schedule, which keeps every pre-round harness
  // byte-identical. It is also DRAW-FREE, so it cannot shift the stream: the
  // whole round's corpus movement comes from the ANSWER changing, never from a
  // different number of draws.
  void refreshHomes(std::int64_t ts) {
    if (relocation_ == nullptr || relocation_->empty()) {
      return;
    }
    for (std::size_t i = 0; i < homeAreas_.size(); ++i) {
      const auto area =
          relocation_->areaAt(static_cast<entity::PersonId>(i + 1), ts);
      if (entity::geography::validArea(area)) {
        homeAreas_[i] = area;
      }
    }
  }

  [[nodiscard]] const entity::parties::relocation::Schedule *
  relocation() const noexcept {
    return relocation_;
  }

  // H2 step 2c: the window day-index from which the retirement
  // consumption step applies; kNoRetirementDay when the person never
  // retires in-window (or no carrier is bound).
  [[nodiscard]] std::uint32_t retirementDay(entity::PersonId p) const noexcept {
    const auto i = index(p);
    return i < retirementDays_.size() ? retirementDays_[i] : kNoRetirementDay;
  }

  // H3: the window day-index of the person's death; kNoDeathDay when
  // the person survives the window (or no carrier is bound).
  [[nodiscard]] std::uint32_t deathDay(entity::PersonId p) const noexcept {
    const auto i = index(p);
    return i < deathDays_.size() ? deathDays_[i] : kNoDeathDay;
  }

private:
  [[nodiscard]] static std::size_t index(entity::PersonId p) noexcept {
    return static_cast<std::size_t>(p - 1);
  }

  std::vector<entity::Key> primary_;
  std::vector<personas::Type> kinds_;
  std::vector<entity::behavior::Persona> objects_;
  Paydays paydays_;
  std::vector<entity::geography::GeoAreaId> homeAreas_;
  std::vector<std::uint32_t> retirementDays_;
  std::vector<std::uint32_t> deathDays_;
  // Non-owning; must outlive the View. Owned by `pipeline::People`, which
  // survives the fold for exactly this reason.
  const entity::parties::relocation::Schedule *relocation_ = nullptr;

  /* Per-person deposit-account rows: `depositOffsets_` has count + 1 entries
   * and bounds each person's slice of `depositAccounts_`. Both empty means
   * "primary only" — see `deposits`. */
  std::vector<std::uint32_t> depositOffsets_;
  std::vector<entity::Key> depositAccounts_;
};

} // namespace PhantomLedger::activity::spending::market::population
