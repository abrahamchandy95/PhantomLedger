#pragma once
//
// phantomledger/synth/pii/membership.hpp
//
// macro-history-v1 H3 part 3c-ii (authority U-8 addendum): the
// MEMBERSHIP INTERVAL. A person is a visible bank customer on
// [joinTs, closeTs):
//
//   joinTs   window start for the seed roster; a drawn join day for
//            the JOIN COHORT (the last K person ids). The join days
//            ride the personas Pack (Pack::joinDays, one draw per
//            joiner on the isolated {"join-cohort", personId} lane —
//            synth/personas/join.hpp derives them and sizes K against
//            the EMBEDDED BEA population series).
//   closeTs  death + kSettlementDays (ACCOUNT CLOSURE). The lag is
//            sized to strictly contain the funeral (death+3-11d) and
//            the estate distribution (death+30-90d), so every estate
//            row is visible before the accounts close.
//
// This class is the pure interval VIEW: it stores the derived join
// days and close epochs and answers activeAt. Construct it through
// synth::personas::join_cohort::membershipOf(pack, window) — the ONE
// construction path every exporter shares (export.cpp, the streaming
// twin, card_fraud, main) so the visible corpus cannot diverge.
//
// The flat 2%/yr `Growth` model (annualRate + splitmix join-day hash)
// is RETIRED — replenishment is BEA-sized and lane-drawn since 3c-ii.
//
// A default-constructed Membership is the FULLY-ACTIVE view (no
// joiners, no closures): the guard shape for packs without carriers.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::pii {

// ACCOUNT CLOSURE settles this many days after death (authority U-8
// addendum: covers the funeral and the 30-90 day estate distribution
// with margin; card servicing additionally stops kCardSettleTailDays
// before this point so no session row posts at/after closure).
inline constexpr int kSettlementDays = 120;

class Membership {
public:
  /// The fully-active view: everyone is a member for the whole window.
  Membership() = default;

  /// `joinDays[p-1]` = the person's join-day offset from window start
  /// (0 = member from the start); `closeEpochs[p-1]` = the epoch at
  /// which the person's accounts close (death + settlement). Both are
  /// PersonId-1 indexed and population-sized; persons beyond the
  /// vectors (counterparties, hubs, invalid owners) are always active.
  Membership(const time::Window &window, std::vector<std::uint32_t> joinDays,
             std::vector<std::int64_t> closeEpochs) noexcept
      : window_(window), startEpoch_(time::toEpochSeconds(window.start)),
        endEpochExcl_(time::toEpochSeconds(window.endExcl())),
        joinDays_(std::move(joinDays)), closeEpochs_(std::move(closeEpochs)) {}

  [[nodiscard]] bool isJoiner(entity::PersonId person) const noexcept {
    return inRange(person) && joinDays_[person - 1] > 0;
  }

  /// The membership start (customer.csv/Party created_at). Window
  /// start for seeds and out-of-range ids.
  [[nodiscard]] time::TimePoint joinTs(entity::PersonId person) const noexcept {
    if (!inRange(person)) {
      return window_.start;
    }
    return window_.start +
           time::Days{static_cast<int>(joinDays_[person - 1])};
  }

  /// The account-closure epoch; int64 max for out-of-range ids and
  /// carrier-less views (never closes).
  [[nodiscard]] std::int64_t
  closeEpoch(entity::PersonId person) const noexcept {
    if (person == 0 || static_cast<std::size_t>(person) > closeEpochs_.size()) {
      return std::numeric_limits<std::int64_t>::max();
    }
    return closeEpochs_[person - 1];
  }

  /// The closure instant when it lands INSIDE the window (the account
  /// is closed at export time); the TimePoint{} sentinel while the
  /// account is still open at window end.
  [[nodiscard]] time::TimePoint
  closedAt(entity::PersonId person) const noexcept {
    const auto close = closeEpoch(person);
    if (close >= endEpochExcl_) {
      return time::TimePoint{};
    }
    return time::fromEpochSeconds(close);
  }

  /// Member at `ts`: joined and not yet closed. Out-of-range ids
  /// (counterparties, hubs, invalid owners) are always active.
  [[nodiscard]] bool activeAt(entity::PersonId person,
                              std::int64_t ts) const noexcept {
    if (!inRange(person)) {
      return true;
    }
    const auto joinEpoch =
        startEpoch_ +
        static_cast<std::int64_t>(joinDays_[person - 1]) * 86'400;
    return ts >= joinEpoch && ts < closeEpochs_[person - 1];
  }

private:
  [[nodiscard]] bool inRange(entity::PersonId person) const noexcept {
    return person != 0 &&
           static_cast<std::size_t>(person) <= joinDays_.size() &&
           static_cast<std::size_t>(person) <= closeEpochs_.size();
  }

  time::Window window_{};
  std::int64_t startEpoch_ = 0;
  std::int64_t endEpochExcl_ = std::numeric_limits<std::int64_t>::max();
  std::vector<std::uint32_t> joinDays_;
  std::vector<std::int64_t> closeEpochs_;
};

} // namespace PhantomLedger::synth::pii
