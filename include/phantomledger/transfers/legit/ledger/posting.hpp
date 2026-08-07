#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <queue>
#include <span>
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

/* ONE DECLINED AUTHORIZATION ATTEMPT, KEPT RATHER THAN COUNTED.
 *
 * `ReplayDropLedger` has always recorded declines — it carries a real
 * taxonomy, cure windows, retry padding and blind-retry delays — but it
 * records COUNTS ONLY, so the attempt itself was discarded at the drop site
 * and nothing survived to export. That is why the corpus contains no declined
 * authorizations at all, and why `card_fraud/derive.hpp`'s `error` column had
 * to be a content hash: its own comment says "authorization attempts are not
 * modelled, so nothing drives this column".
 *
 * The transaction is in hand at every drop site, so keeping it costs one push
 * and no draw. `reason` points at the `drop_reasons` constants, which have
 * static storage, so the view cannot dangle.
 *
 * FUNDING DECLINES ONLY. `kInvalid` and `kUnbooked` mean the book does not
 * know the account — a generator fault, not an authorization outcome — and
 * surfacing those as declines would export a bug as a feature.
 *
 * ------------------------------------------------------------------
 * READ THIS BEFORE EXPORTING ANY OF IT.
 *
 * THIS POPULATION IS MASSIVELY FRAUD-ENRICHED AND EXPORTING IT ALONE WOULD
 * OPEN A SHORTCUT SEVERAL TIMES WORSE THAN THE ONE `device-fanout-2026-08`
 * CLOSED. Measured on capture, four legs: 4,487 / 5,123 / 4,643 / 4,943
 * card-view declines, of which 1,913 / 2,224 / 1,762 / 2,261 carry the fraud
 * flag — 42.6% / 43.4% / 37.9% / 45.7%. Against a settled card-view base rate
 * near 0.96%, "this authorization was declined therefore fraud" would score
 * roughly 0.43 precision at ~45x lift. The degree shortcut this round spent
 * itself closing now sits at 0.03-0.06 precision and 2.8-6.2x.
 *
 * THE CAUSE IS REAL AND SHOULD NOT BE DIALLED AWAY. The unauthorized rail
 * drains a victim across 5-14 charges in a 6-71 hour span, so its own later
 * charges genuinely cannot fund — real card fraud does exhaust an account and
 * get declined. What is wrong is the DENOMINATOR: the model declines
 * legitimate customers only when they run out of money, which is rare,
 * whereas real authorization traffic declines 5-15% of the time and is
 * overwhelmingly legitimate — expired cards, CVV and AVS mismatches, velocity
 * rules, technical failures. Fraud is ~0.1% of real transactions, so it
 * cannot be 43% of real declines.
 *
 * SO THE NON-FUNDING DECLINES ARE LOAD-BEARING FOR SAFETY, NOT GARNISH. They
 * were scoped as the realism half of this work — keeping TabFormer's error
 * mix instead of collapsing it to 100% Insufficient Balance — and the
 * measurement promotes them to the part that makes the export publishable at
 * all. Size them so the funding declines are a MINORITY of the exported
 * decline population, then band the decline population's fraud lift the way
 * sub-gate C bands not-on-file: a ceiling with a lift floor above 1.0,
 * because a declined authorization IS riskier and driving it to 1.0 would
 * replace a shortcut with noise (`attacker-infra-2026-07` rule 5).
 *
 * Nothing here is exported yet. Capturing is inert: measured 66/67 with
 * `golden_run.b2sum` unmoved. */
struct DeclinedAttempt {
  transactions::Transaction txn{};
  std::string_view reason{};
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

  void extendChunk(std::span<const transactions::Transaction> items,
                   std::span<const transactions::Transaction> lookahead,
                   std::int64_t emitBoundExcl);

  [[nodiscard]] std::vector<transactions::Transaction>
  takeSettledBefore(std::int64_t boundExcl);

  /* Fold-residency probes for the `mem` diagnostics: rows the fold retains
   * right now. Settled rows await takeSettledBefore(); pending rows are queued
   * in-flight (future timestamps, retries) at ~136 B each (QueuedItem), so the
   * probe's txn-sized ~MB label understates them by roughly a third. */
  [[nodiscard]] std::size_t settledRows() const noexcept {
    return txns_.size();
  }

  [[nodiscard]] std::size_t pendingRows() const noexcept {
    return pending_.size();
  }

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

  /* The declined attempts, in the order the replay decided them. Retries are
   * INCLUDED as separate entries: each pass through the funding test is one
   * authorization the issuer answered, which is what the exported row means,
   * and the cure/retry machinery already models exactly that sequence. */
  [[nodiscard]] const std::vector<DeclinedAttempt> &
  declined() const noexcept {
    return declined_;
  }

  [[nodiscard]] std::vector<DeclinedAttempt> &&takeDeclined() noexcept {
    return std::move(declined_);
  }

  /* The attempts decided SINCE THE LAST DRAIN, leaving the accumulator empty.
   * The per-span counterpart of takeSettledBefore(), and the reason it exists
   * is the export's ordering law: declined rows are merged into the settled
   * stream BY TIMESTAMP, so a consumer must receive a span's attempts before
   * that span's settled rows rather than the whole run at the end.
   *
   * Definite clear, unlike takeDeclined()'s bare move — a moved-from vector is
   * valid but unspecified, and a stale entry re-drained into the next span
   * would duplicate an exported row. */
  [[nodiscard]] std::vector<DeclinedAttempt> drainDeclined() {
    auto out = std::move(declined_);
    declined_.clear();
    return out;
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
  std::vector<DeclinedAttempt> declined_;

  std::uint64_t nextSequence_ = 0;

  std::uint64_t retrySequence_ = (1ULL << 62U);

  std::priority_queue<QueuedItem, std::vector<QueuedItem>, QueueOrder> pending_;

  void drainPending(std::int64_t emitBoundExcl);
  void indexInbound(const transactions::Transaction &txn);

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

/* ONE SPAN'S DECLINED ATTEMPTS, APPENDED IN TIMESTAMP ORDER.
 *
 * Both replay drivers call this at the same point in their span loop — right
 * after taking the span's settled rows — so the monolithic and windowed
 * engines build byte-identical decline vectors. `test_production_windowed`
 * compares them field-wise for exactly that reason.
 *
 * THE SORT IS WHAT MAKES THE EXPORT'S PREFIX PROPERTY HOLD. The replay decides
 * attempts in retry order, not clock order, and the card-fraud export merges
 * declined rows into the settled stream by timestamp; an out-of-order entry
 * would emit after a settled row it precedes, so a score-time export would
 * stop being a byte prefix of the full one. Stable, so two attempts sharing a
 * timestamp keep the order the replay decided them in. */
inline void appendDeclinedSpan(ChronoReplayAccumulator &accumulator,
                               std::vector<DeclinedAttempt> &out) {
  auto batch = accumulator.drainDeclined();
  std::stable_sort(batch.begin(), batch.end(),
                   [](const DeclinedAttempt &a, const DeclinedAttempt &b) {
                     return a.txn.timestamp < b.txn.timestamp;
                   });
  out.insert(out.end(), std::make_move_iterator(batch.begin()),
             std::make_move_iterator(batch.end()));
}

} // namespace PhantomLedger::transfers::legit::ledger
