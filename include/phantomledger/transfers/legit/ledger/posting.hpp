#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
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

[[nodiscard]] entity::Key bankFeeCollectionKey() noexcept;
[[nodiscard]] entity::Key bankOdLocKey() noexcept;

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

struct ReplayFundingBehavior {
  struct CureWindow {
    std::int32_t cardHours = 10;
    std::int32_t delayedDebitHours = 36;
  };

  struct RetryDelay {
    std::int32_t cardPaddingMinutes = 5;
    std::int32_t debitPaddingMinutes = 30;
    std::int32_t firstBlindHours = 18;
    std::int32_t secondBlindHours = 72;
    double blindProbability = 0.55;
  };

  struct OverdraftFeeBudget {
    std::int32_t dailyCap = 3;
  };

  CureWindow cure{};
  RetryDelay retry{};
  OverdraftFeeBudget overdraftFees{};

  [[nodiscard]] std::int32_t
  maxAttemptsFor(channels::Tag channel) const noexcept;

  [[nodiscard]] std::int32_t cureHoursFor(channels::Tag channel) const noexcept;

  [[nodiscard]] std::int32_t
  paddingMinutesFor(channels::Tag channel) const noexcept;

  [[nodiscard]] std::int32_t
  blindDelayHoursFor(std::int32_t retryCount) const noexcept;
};

class ReplayDropLedger {
public:
  struct ReasonHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
      return std::hash<std::string_view>{}(s);
    }
  };
  struct ReasonEq {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept {
      return a == b;
    }
  };
  using Counts =
      std::unordered_map<std::string, std::uint32_t, ReasonHash, ReasonEq>;

  using ChannelReasonKey = std::pair<std::string, channels::Tag>;
  using ChannelReasonLookupKey = std::pair<std::string_view, channels::Tag>;

  struct ChannelReasonHash {
    using is_transparent = void;
    std::size_t operator()(const ChannelReasonKey &k) const noexcept {
      return mix(std::hash<std::string_view>{}(k.first), k.second.value);
    }
    std::size_t operator()(const ChannelReasonLookupKey &k) const noexcept {
      return mix(std::hash<std::string_view>{}(k.first), k.second.value);
    }

  private:
    static std::size_t mix(std::size_t h1, std::uint8_t tag) noexcept {
      return h1 ^ (static_cast<std::size_t>(tag) + 0x9e37'79b9ULL + (h1 << 6) +
                   (h1 >> 2));
    }
  };
  struct ChannelReasonEq {
    using is_transparent = void;
    bool operator()(const ChannelReasonKey &a,
                    const ChannelReasonKey &b) const noexcept {
      return a.second == b.second && a.first == b.first;
    }
    bool operator()(const ChannelReasonKey &a,
                    const ChannelReasonLookupKey &b) const noexcept {
      return a.second == b.second && a.first == b.first;
    }
    bool operator()(const ChannelReasonLookupKey &a,
                    const ChannelReasonKey &b) const noexcept {
      return a.second == b.second && a.first == b.first;
    }
  };
  using CountsByChannel =
      std::unordered_map<ChannelReasonKey, std::uint32_t, ChannelReasonHash,
                         ChannelReasonEq>;

  void record(std::string_view reason, channels::Tag channel);

  [[nodiscard]] const Counts &byReason() const noexcept { return byReason_; }

  [[nodiscard]] const CountsByChannel &byChannel() const noexcept {
    return byChannel_;
  }

private:
  Counts byReason_;
  CountsByChannel byChannel_;
};

class ChronoReplayAccumulator {
public:
  [[nodiscard]] static constexpr ReplayFundingBehavior
  defaultFundingBehavior() noexcept {
    return {};
  }

  ChronoReplayAccumulator(
      clearing::Ledger *book, random::Rng *rng,
      ReplayFundingBehavior funding = defaultFundingBehavior(),
      bool emitLiquidityEvents = true);

  bool append(const transactions::Transaction &txn);

  void extend(std::vector<transactions::Transaction> items,
              bool presorted = false);

  [[nodiscard]] const std::vector<transactions::Transaction> &
  txns() const noexcept {
    return txns_;
  }

  [[nodiscard]] std::vector<transactions::Transaction> &&takeTxns() noexcept {
    return std::move(txns_);
  }

  [[nodiscard]] const ReplayDropLedger::Counts &dropCounts() const noexcept {
    return drops_.byReason();
  }

  using ChannelReasonKey = ReplayDropLedger::ChannelReasonKey;
  using ChannelReasonHash = ReplayDropLedger::ChannelReasonHash;
  using ChannelReasonEq = ReplayDropLedger::ChannelReasonEq;

  [[nodiscard]] const ReplayDropLedger::CountsByChannel &
  dropCountsByChannel() const noexcept {
    return drops_.byChannel();
  }

  [[nodiscard]] clearing::Ledger *book() const noexcept { return book_; }

private:
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

  clearing::Ledger *book_;
  random::Rng *rng_;
  ReplayFundingBehavior funding_;
  bool emitLiquidityEvents_;

  std::vector<transactions::Transaction> txns_;
  ReplayDropLedger drops_;

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
