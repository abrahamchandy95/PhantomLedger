#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <algorithm>
#include <cassert>
#include <queue>
#include <span>
#include <string>
#include <utility>

namespace PhantomLedger::transfers::legit::ledger {

bool isCureInbound(const transactions::Transaction &txn) noexcept {
  if (encoding::isExternal(txn.source)) {
    return true;
  }

  const auto t = txn.session.channel;

  if (channels::isPaydayInbound(t)) {
    return true;
  }

  if (channels::isFamily(t)) {
    return true;
  }

  if (t == channels::tag(channels::Insurance::claim) ||
      t == channels::tag(channels::Product::taxRefund)) {
    return true;
  }

  if (t == channels::tag(channels::Legit::selfTransfer)) {
    return true;
  }

  if (t == channels::tag(channels::Credit::refund) ||
      t == channels::tag(channels::Credit::chargeback)) {
    return true;
  }

  return false;
}

// ---------------------------------------------------------------------------
// ReplayFundingBehavior
// ---------------------------------------------------------------------------

std::int32_t
ReplayFundingBehavior::maxAttemptsFor(channels::Tag channel) const noexcept {
  if (channels::isCardLike(channel)) {
    return 1;
  }
  if (channels::isRetryable(channel)) {
    return 2;
  }
  return 1;
}

std::int32_t
ReplayFundingBehavior::cureHoursFor(channels::Tag channel) const noexcept {
  return channels::isCardLike(channel) ? cure.cardHours
                                       : cure.delayedDebitHours;
}

std::int32_t
ReplayFundingBehavior::paddingMinutesFor(channels::Tag channel) const noexcept {
  return channels::isCardLike(channel) ? retry.cardPaddingMinutes
                                       : retry.debitPaddingMinutes;
}

std::int32_t ReplayFundingBehavior::blindDelayHoursFor(
    std::int32_t retryCount) const noexcept {
  return retryCount == 0 ? retry.firstBlindHours : retry.secondBlindHours;
}

entity::Key bankFeeCollectionKey() noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         /*number=*/0xFFFF'FF01ULL);
}

entity::Key bankOdLocKey() noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         /*number=*/0xFFFF'FF02ULL);
}

void ReplayDropLedger::record(std::string_view reason, channels::Tag channel) {
  if (auto it = byReason_.find(reason); it != byReason_.end()) {
    ++it->second;
  } else {
    byReason_.emplace(std::string(reason), 1U);
  }

  const ChannelReasonLookupKey lookup{reason, channel};
  if (auto it = byChannel_.find(lookup); it != byChannel_.end()) {
    ++it->second;
  } else {
    byChannel_.emplace(ChannelReasonKey{std::string(reason), channel}, 1U);
  }
}

// ---------------------------------------------------------------------------
// ChronoReplayAccumulator hashes
// ---------------------------------------------------------------------------

std::size_t ChronoReplayAccumulator::FeeKeyHash::operator()(
    const FeeKey &k) const noexcept {
  return (static_cast<std::size_t>(k.first) << 17) ^
         static_cast<std::size_t>(k.second);
}

// ---------------------------------------------------------------------------
// ChronoReplayAccumulator
// ---------------------------------------------------------------------------

ChronoReplayAccumulator::ChronoReplayAccumulator(clearing::Ledger *book,
                                                 random::Rng *rng,
                                                 ReplayFundingBehavior funding,
                                                 bool emitLiquidityEvents)
    : book_(book), rng_(rng), funding_(funding),
      emitLiquidityEvents_(emitLiquidityEvents) {}

bool ChronoReplayAccumulator::append(const transactions::Transaction &txn) {
  if (book_ == nullptr) {
    txns_.push_back(txn);
    return true;
  }

  installLiquiditySink();
  currentTxn_ = &txn;

  const auto srcIdx = book_->findAccount(txn.source);
  const auto dstIdx = book_->findAccount(txn.target);

  const auto decision = book_->transferAt(clearing::Ledger::Posting{
      .srcIdx = srcIdx,
      .dstIdx = dstIdx,
      .amount = txn.amount,
      .channel = txn.session.channel,
      .timestamp = txn.timestamp,
  });

  currentTxn_ = nullptr;
  uninstallLiquiditySink();

  if (decision.accepted()) {
    txns_.push_back(txn);
    return true;
  }

  if (decision.rejectReason().has_value()) {
    const auto reason = *decision.rejectReason();
    switch (reason) {
    case clearing::RejectReason::invalid:
      drops_.record(drop_reasons::kInvalid, txn.session.channel);
      break;
    case clearing::RejectReason::unbooked:
      drops_.record(drop_reasons::kUnbooked, txn.session.channel);
      break;
    case clearing::RejectReason::unfunded:
      drops_.record(drop_reasons::kInsufficientFunds, txn.session.channel);
      break;
    }
  }

  return false;
}

void ChronoReplayAccumulator::extend(
    std::vector<transactions::Transaction> items, bool presorted) {
  if (!presorted) {
    items = sortForReplay(std::span<const transactions::Transaction>(items));
  }

  if (book_ == nullptr) {
    txns_.reserve(txns_.size() + items.size());
    txns_.insert(txns_.end(), std::make_move_iterator(items.begin()),
                 std::make_move_iterator(items.end()));
    return;
  }

  buildFutureInboundIndex(items);
  installLiquiditySink();

  std::priority_queue<QueuedItem, std::vector<QueuedItem>, QueueOrder> pending;

  for (const auto &txn : items) {
    pending.push(QueuedItem{
        .timestamp = txn.timestamp,
        .sequence = nextSequence_++,
        .kind = ItemKind::txn,
        .retryCount = 0,
        .txn = txn,
    });
  }

  while (!pending.empty()) {
    auto item = pending.top();
    pending.pop();

    book_->accrueLocInterestThrough(item.timestamp);

    if (item.kind != ItemKind::txn) {
      continue;
    }

    currentTxn_ = &item.txn;

    const auto srcIdx = book_->findAccount(item.txn.source);
    const auto dstIdx = book_->findAccount(item.txn.target);

    const auto decision = book_->transferAt(clearing::Ledger::Posting{
        .srcIdx = srcIdx,
        .dstIdx = dstIdx,
        .amount = item.txn.amount,
        .channel = item.txn.session.channel,
        .timestamp = item.txn.timestamp,
    });

    currentTxn_ = nullptr;

    if (decision.accepted()) {
      txns_.push_back(item.txn);
      continue;
    }

    if (!decision.rejectReason().has_value()) {
      continue;
    }

    const auto reason = *decision.rejectReason();
    if (reason != clearing::RejectReason::unfunded) {
      const auto reasonStr = (reason == clearing::RejectReason::invalid)
                                 ? drop_reasons::kInvalid
                                 : drop_reasons::kUnbooked;
      drops_.record(reasonStr, item.txn.session.channel);
      continue;
    }

    const auto retryTs = resolveRetryTimestamp(item.txn, item.retryCount);
    if (retryTs == 0) {
      drops_.record(terminalReason(item.txn.session.channel),
                    item.txn.session.channel);
      continue;
    }

    if (item.retryCount + 1 >
        funding_.maxAttemptsFor(item.txn.session.channel)) {
      drops_.record(drop_reasons::kInsufficientFundsRetryExhausted,
                    item.txn.session.channel);
      continue;
    }

    auto retried = item.txn;
    retried.timestamp = retryTs;

    pending.push(QueuedItem{
        .timestamp = retryTs,
        .sequence = nextSequence_++,
        .kind = ItemKind::txn,
        .retryCount = item.retryCount + 1,
        .txn = retried,
    });
  }

  if (!items.empty()) {
    book_->accrueLocInterestThrough(items.back().timestamp + 1);
  }

  uninstallLiquiditySink();
  futureInboundTimes_.clear();

  std::sort(txns_.begin(), txns_.end(),
            [](const transactions::Transaction &a,
               const transactions::Transaction &b) noexcept {
              if (a.timestamp != b.timestamp) {
                return a.timestamp < b.timestamp;
              }
              if (a.source != b.source) {
                return a.source < b.source;
              }
              if (a.target != b.target) {
                return a.target < b.target;
              }
              return a.amount < b.amount;
            });
}

// ---------------------------------------------------------------------------
// Cure-window machinery
// ---------------------------------------------------------------------------

void ChronoReplayAccumulator::buildFutureInboundIndex(
    const std::vector<transactions::Transaction> &items) {
  futureInboundTimes_.clear();

  for (const auto &txn : items) {
    if (!isCureInbound(txn)) {
      continue;
    }
    futureInboundTimes_[txn.target].push_back(txn.timestamp);
  }
}

std::int64_t ChronoReplayAccumulator::resolveRetryTimestamp(
    const transactions::Transaction &txn, std::int32_t retryCount) {
  const auto cureTs = findFutureCure(txn);
  if (cureTs != 0) {
    return cureTs + static_cast<std::int64_t>(
                        funding_.paddingMinutesFor(txn.session.channel)) *
                        60;
  }

  if (rng_ == nullptr || !channels::isRetryable(txn.session.channel) ||
      retryCount >= funding_.maxAttemptsFor(txn.session.channel)) {
    return 0;
  }

  if (!rng_->coin(funding_.retry.blindProbability)) {
    return 0;
  }

  return txn.timestamp +
         static_cast<std::int64_t>(funding_.blindDelayHoursFor(retryCount)) *
             3600;
}

std::int64_t ChronoReplayAccumulator::findFutureCure(
    const transactions::Transaction &txn) const {
  const auto it = futureInboundTimes_.find(txn.source);
  if (it == futureInboundTimes_.end() || it->second.empty()) {
    return 0;
  }

  const auto upper =
      txn.timestamp +
      static_cast<std::int64_t>(funding_.cureHoursFor(txn.session.channel)) *
          3600;

  const auto &times = it->second;
  auto pos = std::upper_bound(times.begin(), times.end(), txn.timestamp);
  if (pos == times.end()) {
    return 0;
  }
  if (*pos > upper) {
    return 0;
  }
  return *pos;
}

std::string_view
ChronoReplayAccumulator::terminalReason(channels::Tag channel) noexcept {
  if (channels::isCardLike(channel)) {
    return drop_reasons::kInsufficientFundsInstantDecline;
  }
  if (channels::isRetryable(channel)) {
    return drop_reasons::kInsufficientFundsTerminal;
  }
  return drop_reasons::kInsufficientFunds;
}

// ---------------------------------------------------------------------------
// LiquiditySink wiring + overdraft fee daily cap
// ---------------------------------------------------------------------------

void ChronoReplayAccumulator::installLiquiditySink() {
  if (book_ == nullptr) {
    return;
  }

  book_->setEmitLiquidity(emitLiquidityEvents_);

  if (!emitLiquidityEvents_) {
    book_->setLiquiditySink({});
    return;
  }

  book_->setLiquiditySink([this](const clearing::LiquidityEvent &event) {
    onLiquidityEvent(event);
  });
}

void ChronoReplayAccumulator::uninstallLiquiditySink() {
  if (book_ == nullptr) {
    return;
  }

  book_->setLiquiditySink({});
}

bool ChronoReplayAccumulator::feeBudgetAllows(
    const clearing::LiquidityEvent &event) noexcept {
  if (event.channel != channels::tag(channels::Liquidity::overdraftFee)) {
    return true;
  }

  const auto srcIdx = (book_ != nullptr) ? book_->findAccount(event.payerKey)
                                         : clearing::Ledger::invalid;
  if (srcIdx == clearing::Ledger::invalid) {
    return true;
  }

  const auto day = static_cast<std::int32_t>(
      event.timestamp / ::PhantomLedger::time::kSecondsPerDay);
  const FeeKey key{srcIdx, day};
  auto &count = feeTapsToday_[key];

  if (count >= funding_.overdraftFees.dailyCap) {
    return false;
  }

  ++count;
  return true;
}

void ChronoReplayAccumulator::onLiquidityEvent(
    const clearing::LiquidityEvent &event) {
  if (!emitLiquidityEvents_) {
    return;
  }

  if (!feeBudgetAllows(event)) {
    return;
  }

  const auto target =
      (event.channel == channels::tag(channels::Liquidity::overdraftFee))
          ? bankFeeCollectionKey()
          : bankOdLocKey();

  transactions::Transaction tx{};
  tx.source = event.payerKey;
  tx.target = target;
  tx.amount = event.amount;

  tx.timestamp =
      (event.channel == channels::tag(channels::Liquidity::overdraftFee))
          ? event.timestamp + 1
          : event.timestamp;

  tx.fraud.flag = 0;
  tx.fraud.ringId.reset();
  tx.session.channel = event.channel;

  if (currentTxn_ != nullptr) {
    tx.session.deviceId = currentTxn_->session.deviceId;
    tx.session.ipAddress = currentTxn_->session.ipAddress;
  }

  txns_.push_back(std::move(tx));
}

} // namespace PhantomLedger::transfers::legit::ledger
