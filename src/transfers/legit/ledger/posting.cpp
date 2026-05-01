#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <algorithm>
#include <cassert>
#include <queue>
#include <string>

namespace PhantomLedger::transfers::legit::ledger {

[[nodiscard]] static bool isCardLike(channels::Tag t) noexcept {
  using channels::Legit;
  if (t == channels::tag(Legit::atm) || t == channels::tag(Legit::merchant) ||
      t == channels::tag(Legit::cardPurchase) ||
      t == channels::tag(Legit::p2p)) {
    return true;
  }
  return false;
}

[[nodiscard]] static bool isRetryable(channels::Tag t) noexcept {
  using channels::Insurance;
  using channels::Legit;
  using channels::Product;

  if (t == channels::tag(Legit::bill) ||
      t == channels::tag(Legit::subscription) ||
      t == channels::tag(Legit::externalUnknown)) {
    return true;
  }
  if (channels::isRent(t)) {
    return true;
  }
  if (t == channels::tag(Insurance::premium)) {
    return true;
  }
  if (t == channels::tag(Product::mortgage) ||
      t == channels::tag(Product::autoLoan) ||
      t == channels::tag(Product::studentLoan) ||
      t == channels::tag(Product::taxEstimated)) {
    return true;
  }
  return false;
}

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

  // Insurance claims and tax refunds.
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
// ReplayPolicy
// ---------------------------------------------------------------------------

std::int32_t ReplayPolicy::maxRetriesFor(channels::Tag channel) const noexcept {
  if (isCardLike(channel)) {
    return 1;
  }
  if (isRetryable(channel)) {
    return 2;
  }
  return 1;
}

// ---------------------------------------------------------------------------
// System counterparty keys for liquidity-event Transactions
// ---------------------------------------------------------------------------

entity::Key bankFeeCollectionKey() noexcept {

  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         /*number=*/0xFFFF'FF01ULL);
}

entity::Key bankOdLocKey() noexcept {
  return entity::makeKey(entity::Role::business, entity::Bank::external,
                         /*number=*/0xFFFF'FF02ULL);
}

// ---------------------------------------------------------------------------
// Hashes
// ---------------------------------------------------------------------------

std::size_t ChronoReplayAccumulator::ChannelReasonHash::operator()(
    const ChannelReasonKey &k) const noexcept {
  const std::size_t h1 = std::hash<std::string>{}(k.first);
  const std::size_t h2 = std::hash<std::string>{}(k.second);
  return h1 ^ (h2 + 0x9e37'79b9ULL + (h1 << 6) + (h1 >> 2));
}

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
                                                 ReplayPolicy policy,
                                                 bool emitLiquidityEvents)
    : book_(book), rng_(rng), policy_(policy),
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

  const auto decision = book_->transferAt(srcIdx, dstIdx, txn.amount,
                                          txn.session.channel, txn.timestamp);

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
      recordDrop(drop_reasons::kInvalid, txn.session.channel);
      break;
    case clearing::RejectReason::unbooked:
      recordDrop(drop_reasons::kUnbooked, txn.session.channel);
      break;
    case clearing::RejectReason::unfunded:
      recordDrop(drop_reasons::kInsufficientFunds, txn.session.channel);
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
      continue; // reserved for future LOC-billing kinds
    }

    currentTxn_ = &item.txn;

    const auto srcIdx = book_->findAccount(item.txn.source);
    const auto dstIdx = book_->findAccount(item.txn.target);

    const auto decision =
        book_->transferAt(srcIdx, dstIdx, item.txn.amount,
                          item.txn.session.channel, item.txn.timestamp);

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
      // invalid / unbooked: terminal, no retry path is sensible.
      const auto reasonStr = (reason == clearing::RejectReason::invalid)
                                 ? drop_reasons::kInvalid
                                 : drop_reasons::kUnbooked;
      recordDrop(reasonStr, item.txn.session.channel);
      continue;
    }

    // ---- Cure / blind retry ----
    const auto retryTs = resolveRetryTimestamp(item.txn, item.retryCount);
    if (retryTs == 0) {
      recordDrop(terminalReason(item.txn.session.channel),
                 item.txn.session.channel);
      continue;
    }

    if (item.retryCount + 1 > policy_.maxRetriesFor(item.txn.session.channel)) {
      recordDrop(drop_reasons::kInsufficientFundsRetryExhausted,
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

  // Final LOC sweep past the last seen timestamp so any periods that
  // matured between the last txn and the simulation horizon get
  // emitted.
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
// Drop bookkeeping
// ---------------------------------------------------------------------------

void ChronoReplayAccumulator::recordDrop(std::string_view reason,
                                         channels::Tag channel) {
  const std::string reasonStr(reason);
  ++dropCounts_[reasonStr];

  std::string channelName(channels::name(channel));
  if (channelName.empty()) {
    channelName = "none";
  }

  ++dropCountsByChannel_[ChannelReasonKey{std::move(channelName), reasonStr}];
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
    const auto paddingMinutes = isCardLike(txn.session.channel)
                                    ? policy_.cardRetryPaddingMinutes
                                    : policy_.achRetryPaddingMinutes;
    return cureTs + static_cast<std::int64_t>(paddingMinutes) * 60;
  }

  if (rng_ == nullptr || !isRetryable(txn.session.channel) ||
      retryCount >= policy_.maxRetriesFor(txn.session.channel)) {
    return 0;
  }

  if (!rng_->coin(policy_.blindRetryProbability)) {
    return 0;
  }

  const auto delayHours = (retryCount == 0)
                              ? policy_.blindRetryDelayHours
                              : policy_.blindRetrySecondDelayHours;
  return txn.timestamp + static_cast<std::int64_t>(delayHours) * 3600;
}

std::int64_t ChronoReplayAccumulator::findFutureCure(
    const transactions::Transaction &txn) const {
  const auto it = futureInboundTimes_.find(txn.source);
  if (it == futureInboundTimes_.end() || it->second.empty()) {
    return 0;
  }

  const auto cureHours = isCardLike(txn.session.channel)
                             ? policy_.sameDayCureHours
                             : policy_.delayedCureHours;
  const auto upper =
      txn.timestamp + static_cast<std::int64_t>(cureHours) * 3600;

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
  if (isCardLike(channel)) {
    return drop_reasons::kInsufficientFundsInstantDecline;
  }
  if (isRetryable(channel)) {
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

  // Capture by raw `this` — the lambda's lifetime is bounded by
  // {install,uninstall}LiquiditySink and the accumulator outlives both
  // calls.
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
  // Only overdraft-fee events are budgeted. LOC interest is allowed
  // through unconditionally.
  if (event.channel != channels::tag(channels::Liquidity::overdraftFee)) {
    return true;
  }

  const auto srcIdx = (book_ != nullptr) ? book_->findAccount(event.payerKey)
                                         : clearing::Ledger::invalid;
  if (srcIdx == clearing::Ledger::invalid) {
    return true;
  }

  const auto day = static_cast<std::int32_t>(event.timestamp / 86400);
  const FeeKey key{srcIdx, day};
  auto &count = feeTapsToday_[key];
  if (count >= policy_.overdraftFeeDailyCap) {
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

  // Pick the system-internal counterparty for this fee/interest.
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
