#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

namespace PhantomLedger::clearing {

namespace {

namespace boundary = ::PhantomLedger::entity::boundary;

[[nodiscard]] constexpr bool isExternalAccount(const entity::Key &id) noexcept {
  return id.bank == entity::Bank::external;
}

struct BoundaryRequirement {
  boundary::Kind kind = boundary::Kind::genericCounterparty;
  boundary::Flow flow = boundary::Flow::inbound;
};

[[nodiscard]] constexpr std::optional<BoundaryRequirement>
boundaryRequirement(channels::Tag channel) noexcept {
  if (channels::is(channel, channels::Legit::atm)) {
    return BoundaryRequirement{boundary::Kind::atmTerminal,
                               boundary::Flow::outbound};
  }
  if (channels::is(channel, channels::Legit::cashDeposit)) {
    return BoundaryRequirement{boundary::Kind::cashDepository,
                               boundary::Flow::inbound};
  }
  if (channels::is(channel, channels::Deposit::checkDeposit)) {
    return BoundaryRequirement{boundary::Kind::checkCapture,
                               boundary::Flow::inbound};
  }
  if (channels::is(channel, channels::Crypto::rampOut)) {
    return BoundaryRequirement{boundary::Kind::cryptoVenue,
                               boundary::Flow::outbound};
  }
  if (channels::is(channel, channels::Crypto::rampIn)) {
    return BoundaryRequirement{boundary::Kind::cryptoVenue,
                               boundary::Flow::inbound};
  }
  return std::nullopt;
}

[[nodiscard]] constexpr bool
allowsTypedBoundary(boundary::Policy policy, channels::Tag channel,
                    boundary::Flow actualFlow) noexcept {
  const auto required = boundaryRequirement(channel);
  if (!boundary::typed(policy)) {
    // Generic counterparties retain their legacy one-sided semantics only for
    // channels that do not declare a stricter boundary contract. A generic
    // key must not be able to impersonate an ATM, check-capture point, or
    // crypto venue.
    return !required.has_value();
  }

  // Conversely, a typed service endpoint may not escape its contract by
  // riding an unrelated generic channel.
  return required.has_value() && required->kind == policy.kind &&
         required->flow == actualFlow && boundary::allows(policy, actualFlow);
}

struct OverdraftFeeAssessment {
  bool fires = false;
  double amount = 0.0;

  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return fires;
  }
};

struct CashTransition {
  double before = 0.0;
  double after = 0.0;

  [[nodiscard]] constexpr bool crossed() const noexcept {
    return before >= 0.0 && after < 0.0;
  }
};

[[nodiscard]] constexpr OverdraftFeeAssessment
assessOverdraftFee(ProtectionType protection, channels::Tag channel,
                   CashTransition transition, double feeAmount) noexcept {
  if (protection != ProtectionType::courtesy &&
      protection != ProtectionType::linked) {
    return {};
  }

  if (channels::isLiquidity(channel) || feeAmount <= 0.0) {
    return {};
  }

  if (!transition.crossed()) {
    return {};
  }

  return OverdraftFeeAssessment{.fires = true, .amount = feeAmount};
}

} // namespace

void Ledger::initialize(Index count) {
  size_ = count;

  cash_.assign(count, 0.0);
  overdrafts_.assign(count, 0.0);
  linked_.assign(count, 0.0);
  courtesy_.assign(count, 0.0);

  internalAccounts_.clear();
  internalAccounts_.reserve(count);
  externalAccounts_.clear();
  externalAccounts_.reserve(count);
  accountKeys_.assign(count, entity::Key{});

  protectionType_.assign(count, ProtectionType::none);
  bankTier_.assign(count, BankTier::zeroFee);
  overdraftFeeAmount_.assign(count, 0.0);

  locTracker_.initialize(count);

  emitLiquidity_ = true;
  liquiditySink_ = nullptr;
}

void Ledger::addAccount(const entity::Key &id, Index idx) {
  addAccount(id, idx, entity::boundary::kGenericPolicy);
}

void Ledger::addAccount(const entity::Key &id, Index idx,
                        entity::boundary::Policy boundaryPolicy) {
  assert(idx < size_);
  accountKeys_[idx] = id;
  if (isExternalAccount(id)) {
    externalAccounts_.insert_or_assign(id, boundaryPolicy);
  } else {
    internalAccounts_.insert_or_assign(id, idx);
  }
}

bool Ledger::isValid(Index idx) const noexcept {
  return idx != invalid && idx < size_;
}

double &Ledger::cash(Index idx) noexcept {
  assert(idx < size_);
  return cash_[idx];
}

double &Ledger::overdraft(Index idx) noexcept {
  assert(idx < size_);
  return overdrafts_[idx];
}

double &Ledger::linked(Index idx) noexcept {
  assert(idx < size_);
  return linked_[idx];
}

double &Ledger::courtesy(Index idx) noexcept {
  assert(idx < size_);
  return courtesy_[idx];
}

double Ledger::totalLiquidity(Index idx) const noexcept {
  assert(idx < size_);
  return cash_[idx] + overdrafts_[idx] + linked_[idx] + courtesy_[idx];
}

double Ledger::liquidity(Index idx) const noexcept {
  if (!isValid(idx)) {
    return 0.0;
  }
  return totalLiquidity(idx);
}

double Ledger::availableCash(Index idx) const noexcept {
  if (!isValid(idx)) {
    return 0.0;
  }
  return cash_[idx];
}

double Ledger::liquidity(const entity::Key &identity) const {
  return liquidity(findAccount(identity));
}

double Ledger::availableCash(const entity::Key &identity) const {
  return availableCash(findAccount(identity));
}

Ledger::Index Ledger::findAccount(const entity::Key &identity) const {
  const auto it = internalAccounts_.find(identity);
  return it == internalAccounts_.end() ? invalid : it->second;
}

void Ledger::setOverdraftOnly(Index idx, double limit) noexcept {
  assert(idx < size_);
  cash_[idx] = 0.0;
  overdrafts_[idx] = limit;
  linked_[idx] = 0.0;
  courtesy_[idx] = 0.0;
  // Transitioning to courtesy, so make sure LOC tracking is off.
  locTracker_.disable(idx);
  protectionType_[idx] =
      limit > 0.0 ? ProtectionType::courtesy : ProtectionType::none;
}

// --- Protection / tier / LOC setup ---

void Ledger::setProtection(Index idx, ProtectionType type,
                           double bufferAmount) noexcept {
  assert(idx < size_);

  overdrafts_[idx] = 0.0;
  linked_[idx] = 0.0;
  courtesy_[idx] = 0.0;

  protectionType_[idx] = type;

  switch (type) {
  case ProtectionType::none:
    locTracker_.disable(idx);
    break;
  case ProtectionType::courtesy:
    courtesy_[idx] = bufferAmount;
    locTracker_.disable(idx);
    break;
  case ProtectionType::linked:
    linked_[idx] = bufferAmount;
    locTracker_.disable(idx);
    break;
  case ProtectionType::loc:
    overdrafts_[idx] = bufferAmount;
    locTracker_.enable(idx, 0.0, 0);
    break;
  }
}

void Ledger::setBankTier(Index idx, BankTier tier, double feeAmount) noexcept {
  assert(idx < size_);
  bankTier_[idx] = tier;
  overdraftFeeAmount_[idx] = feeAmount;
}

void Ledger::setLoc(Index idx, double apr, int billingDay) noexcept {
  assert(idx < size_);
  if (protectionType_[idx] != ProtectionType::loc) {
    return;
  }
  locTracker_.enable(idx, apr, billingDay);
}

ProtectionType Ledger::protectionType(Index idx) const noexcept {
  assert(idx < size_);
  return protectionType_[idx];
}

BankTier Ledger::bankTier(Index idx) const noexcept {
  assert(idx < size_);
  return bankTier_[idx];
}

double Ledger::overdraftFeeAmount(Index idx) const noexcept {
  assert(idx < size_);
  return overdraftFeeAmount_[idx];
}

// --- Liquidity sink / mode ---

void Ledger::setLiquiditySink(LiquiditySink sink) noexcept {
  liquiditySink_ = std::move(sink);
}

void Ledger::setEmitLiquidity(bool emit) noexcept { emitLiquidity_ = emit; }

// --- Core transfer logic ---

TransferDecision Ledger::decide(Index srcIdx, Index dstIdx, double amount,
                                channels::Tag channel) const noexcept {
  if (amount <= 0.0 || !std::isfinite(amount)) {
    return TransferDecision::reject(RejectReason::invalid);
  }

  const bool srcExternal = (srcIdx == invalid);
  const bool dstExternal = (dstIdx == invalid);

  if (srcExternal && dstExternal) {
    return TransferDecision::reject(RejectReason::unbooked);
  }
  if (srcExternal) {
    return TransferDecision::accept();
  }

  if (!channels::isLiquidity(channel)) {
    const bool selfTransfer =
        channels::is(channel, channels::Legit::selfTransfer);
    const double spendable =
        selfTransfer ? cash_[srcIdx] : totalLiquidity(srcIdx);
    if (spendable < amount) {
      return TransferDecision::reject(RejectReason::unfunded);
    }
  }

  return TransferDecision::accept();
}

TransferDecision Ledger::applyTransfer(const Posting &posting,
                                       double &srcCashBefore) noexcept {
  srcCashBefore = 0.0;

  const auto decision =
      decide(posting.srcIdx, posting.dstIdx, posting.amount, posting.channel);
  if (decision.rejected()) {
    return decision;
  }

  const bool srcExternal = (posting.srcIdx == invalid);
  const bool dstExternal = (posting.dstIdx == invalid);

  if (srcExternal) {
    cash_[posting.dstIdx] += posting.amount;
    return decision;
  }

  srcCashBefore = cash_[posting.srcIdx];

  cash_[posting.srcIdx] -= posting.amount;
  if (!dstExternal) {
    cash_[posting.dstIdx] += posting.amount;
  }
  return decision;
}

TransferDecision Ledger::transfer(Index srcIdx, Index dstIdx, double amount,
                                  channels::Tag channel) noexcept {
  double srcCashBefore = 0.0;
  return applyTransfer(Posting{.srcIdx = srcIdx,
                               .dstIdx = dstIdx,
                               .amount = amount,
                               .channel = channel,
                               .timestamp = 0},
                       srcCashBefore);
}

TransferDecision Ledger::transfer(const entity::Key &src,
                                  const entity::Key &dst, double amount,
                                  channels::Tag channel) {
  return transferAt(KeyPosting{
      .source = src,
      .destination = dst,
      .amount = amount,
      .channel = channel,
      .timestamp = 0,
  });
}

TransferDecision Ledger::transferAt(const KeyPosting &posting) {
  if (!entity::valid(posting.source) || !entity::valid(posting.destination)) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  const bool srcExternal = isExternalAccount(posting.source);
  const bool dstExternal = isExternalAccount(posting.destination);

  const auto srcBoundary = srcExternal ? externalAccounts_.find(posting.source)
                                       : externalAccounts_.end();
  const auto dstBoundary = dstExternal
                               ? externalAccounts_.find(posting.destination)
                               : externalAccounts_.end();

  if ((srcExternal && srcBoundary == externalAccounts_.end()) ||
      (dstExternal && dstBoundary == externalAccounts_.end())) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  if ((srcExternal && !allowsTypedBoundary(srcBoundary->second, posting.channel,
                                           entity::boundary::Flow::inbound)) ||
      (dstExternal && !allowsTypedBoundary(dstBoundary->second, posting.channel,
                                           entity::boundary::Flow::outbound))) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  const Index srcIdx = srcExternal ? invalid : findAccount(posting.source);
  const Index dstIdx = dstExternal ? invalid : findAccount(posting.destination);

  if (!srcExternal && srcIdx == invalid) {
    return TransferDecision::reject(RejectReason::unbooked);
  }
  if (!dstExternal && dstIdx == invalid) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  return transferAt(Posting{
      .srcIdx = srcIdx,
      .dstIdx = dstIdx,
      .amount = posting.amount,
      .channel = posting.channel,
      .timestamp = posting.timestamp,
  });
}

void Ledger::debitAndEmit(Index idx, double amount, channels::Tag channel,
                          std::int64_t timestamp) {
  /* Roll the integral to `timestamp` on the PRE-debit balance before moving
   * cash. The LOC integral only advances where it is told to, and this is the
   * one cash write that does not funnel through `transferAt`'s update pair —
   * dropping this call loses a whole accrual interval. A no-op when the caller
   * already stamped this slot at this instant, which both existing paths do. */
  locTracker_.update(idx, cash_[idx], timestamp);

  /* Bypass the funding check: fee collection / interest must apply against
   * already-negative balances. */
  cash_[idx] -= amount;

  if (liquiditySink_) {
    liquiditySink_(LiquidityEvent{
        .channel = channel,
        .payerKey = accountKeys_[idx],
        .amount = amount,
        .timestamp = timestamp,
    });
  }
}

TransferDecision Ledger::transferAt(const Posting &posting) noexcept {
  // Roll LOC integrals forward on both legs using pre-transfer cash.
  if (isValid(posting.srcIdx)) {
    locTracker_.update(posting.srcIdx, cash_[posting.srcIdx],
                       posting.timestamp);
  }
  if (isValid(posting.dstIdx)) {
    locTracker_.update(posting.dstIdx, cash_[posting.dstIdx],
                       posting.timestamp);
  }

  double srcCashBefore = 0.0;
  const auto decision = applyTransfer(posting, srcCashBefore);

  if (decision.rejected() || !emitLiquidity_ || posting.srcIdx == invalid) {
    return decision;
  }

  // Credit-card limits reuse the overdraft-capacity fields, but crossing
  // below zero on a revolving card is ordinary utilization, not a checking
  // overdraft. Keep the generic overdraft-only/courtesy behavior for every
  // non-card account while suppressing that category error for card sources.
  const bool creditCardSource =
      accountKeys_[posting.srcIdx].role == entity::Role::card;
  const auto fee =
      creditCardSource
          ? OverdraftFeeAssessment{}
          : assessOverdraftFee(protectionType_[posting.srcIdx], posting.channel,
                               CashTransition{.before = srcCashBefore,
                                              .after = cash_[posting.srcIdx]},
                               overdraftFeeAmount_[posting.srcIdx]);
  if (fee) {
    debitAndEmit(posting.srcIdx, fee.amount,
                 channels::tag(channels::Liquidity::overdraftFee),
                 posting.timestamp);
  }

  return decision;
}

void Ledger::accrueLocInterestThrough(std::int64_t timestamp) noexcept {
  std::vector<InterestAccrual> matured;
  locTracker_.sweep(
      timestamp, [this](Index idx) noexcept { return cash_[idx]; }, matured);

  if (!emitLiquidity_) {
    return;
  }

  const auto channel = channels::tag(channels::Liquidity::locInterest);
  for (const auto &a : matured) {
    debitAndEmit(a.accountIndex, a.interest, channel, a.timestamp);
  }
}

Ledger Ledger::clone() const { return *this; }

void Ledger::restore(const Ledger &other) {
  assert(size_ == other.size_);

  std::copy(other.cash_.begin(), other.cash_.end(), cash_.begin());
  std::copy(other.overdrafts_.begin(), other.overdrafts_.end(),
            overdrafts_.begin());
  std::copy(other.linked_.begin(), other.linked_.end(), linked_.begin());
  std::copy(other.courtesy_.begin(), other.courtesy_.end(), courtesy_.begin());

  locTracker_.copyStateFrom(other.locTracker_);
}

} // namespace PhantomLedger::clearing
