#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::transfers::legit::ledger {

// ---------------------------------------------------------------------------
// Policy
// ---------------------------------------------------------------------------

struct ReplayPolicy {
  // Cure-window lookahead.
  std::int32_t sameDayCureHours = 10;
  std::int32_t delayedCureHours = 36;

  // Retry-padding after a cure.
  std::int32_t cardRetryPaddingMinutes = 5;
  std::int32_t achRetryPaddingMinutes = 30;

  std::int32_t blindRetryDelayHours = 18;
  std::int32_t blindRetrySecondDelayHours = 72;
  double blindRetryProbability = 0.55;

  std::int32_t overdraftFeeDailyCap = 3;

  [[nodiscard]] std::int32_t
  maxRetriesFor(channels::Tag channel) const noexcept;
};

inline constexpr ReplayPolicy kDefaultReplayPolicy{};

// ---------------------------------------------------------------------------
// System counterparties for liquidity-event Transactions
// ---------------------------------------------------------------------------
[[nodiscard]] entity::Key bankFeeCollectionKey() noexcept;
[[nodiscard]] entity::Key bankOdLocKey() noexcept;

// ---------------------------------------------------------------------------
// Drop reasons
// ---------------------------------------------------------------------------

namespace drop_reasons {

inline constexpr std::string_view kInsufficientFunds = "insufficient_funds";
inline constexpr std::string_view kInsufficientFundsRetryExhausted =
    "insufficient_funds_retry_exhausted";
inline constexpr std::string_view kInsufficientFundsTerminal =
    "insufficient_funds_terminal";
inline constexpr std::string_view kInsufficientFundsInstantDecline =
    "insufficient_funds_instant_decline";
inline constexpr std::string_view kInvalid = "invalid";
inline constexpr std::string_view kUnbooked = "unbooked";

} // namespace drop_reasons

// ---------------------------------------------------------------------------
// Accumulator
// ---------------------------------------------------------------------------

class ChronoReplayAccumulator {
public:
  ChronoReplayAccumulator(clearing::Ledger *book, random::Rng *rng,
                          ReplayPolicy policy = kDefaultReplayPolicy,
                          bool emitLiquidityEvents = true);

  bool append(const transactions::Transaction &txn);

  void extend(std::vector<transactions::Transaction> items,
              bool presorted = false);

  // ---- Outputs ----

  [[nodiscard]] const std::vector<transactions::Transaction> &
  txns() const noexcept {
    return txns_;
  }

  [[nodiscard]] std::vector<transactions::Transaction> &&takeTxns() noexcept {
    return std::move(txns_);
  }

  [[nodiscard]] const std::unordered_map<std::string, std::uint32_t> &
  dropCounts() const noexcept {
    return dropCounts_;
  }

  using ChannelReasonKey = std::pair<std::string, std::string>;
  struct ChannelReasonHash {
    std::size_t operator()(const ChannelReasonKey &k) const noexcept;
  };

  [[nodiscard]] const std::unordered_map<ChannelReasonKey, std::uint32_t,
                                         ChannelReasonHash> &
  dropCountsByChannel() const noexcept {
    return dropCountsByChannel_;
  }

  [[nodiscard]] clearing::Ledger *book() const noexcept { return book_; }

private:
  // ---- Heap entry ----
  enum class ItemKind : std::uint8_t {
    txn = 0,
    locBilling = 1,
  };

  struct QueuedItem {
    std::int64_t timestamp = 0;
    std::uint64_t sequence = 0;
    ItemKind kind = ItemKind::txn;
    std::int32_t retryCount = 0;
    transactions::Transaction txn{};
  };

  struct QueueOrder {
    bool operator()(const QueuedItem &a, const QueuedItem &b) const noexcept {
      if (a.timestamp != b.timestamp) {
        return a.timestamp > b.timestamp;
      }
      return a.sequence > b.sequence;
    }
  };

  // ---- Drop bookkeeping ----
  void recordDrop(std::string_view reason, channels::Tag channel);

  // ---- Cure-window machinery ----
  void
  buildFutureInboundIndex(const std::vector<transactions::Transaction> &items);

  [[nodiscard]] std::int64_t
  resolveRetryTimestamp(const transactions::Transaction &txn,
                        std::int32_t retryCount);

  [[nodiscard]] std::int64_t
  findFutureCure(const transactions::Transaction &txn) const;

  [[nodiscard]] static std::string_view
  terminalReason(channels::Tag channel) noexcept;

  void installLiquiditySink();
  void uninstallLiquiditySink();

  void onLiquidityEvent(const clearing::LiquidityEvent &event);

  [[nodiscard]] bool
  feeBudgetAllows(const clearing::LiquidityEvent &event) noexcept;

  // ---- State ----
  clearing::Ledger *book_;
  random::Rng *rng_;
  ReplayPolicy policy_;
  bool emitLiquidityEvents_;

  std::vector<transactions::Transaction> txns_;
  std::unordered_map<std::string, std::uint32_t> dropCounts_;
  std::unordered_map<ChannelReasonKey, std::uint32_t, ChannelReasonHash>
      dropCountsByChannel_;

  std::uint64_t nextSequence_ = 0;

  std::unordered_map<entity::Key, std::vector<std::int64_t>>
      futureInboundTimes_;

  using FeeKey = std::pair<clearing::Ledger::Index, std::int32_t>;
  struct FeeKeyHash {
    std::size_t operator()(const FeeKey &k) const noexcept;
  };
  std::unordered_map<FeeKey, std::int32_t, FeeKeyHash> feeTapsToday_;

  const transactions::Transaction *currentTxn_ = nullptr;
};

[[nodiscard]] bool isCureInbound(const transactions::Transaction &txn) noexcept;

} // namespace PhantomLedger::transfers::legit::ledger
