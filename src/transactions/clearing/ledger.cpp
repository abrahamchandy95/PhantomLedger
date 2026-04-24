#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/taxonomies/channels/predicates.hpp"
#include "phantomledger/transactions/clearing/overdraft_fee_policy.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

namespace PhantomLedger::clearing {

namespace {

[[nodiscard]] constexpr bool isExternalAccount(const entity::Key &id) noexcept {
  return id.bank == entity::Bank::external;
}

} // namespace

void Ledger::initialize(Index count) {
  size_ = count;

  cash_.assign(count, 0.0);
  overdrafts_.assign(count, 0.0);
  linked_.assign(count, 0.0);
  courtesy_.assign(count, 0.0);
  flags_.assign(count, none);

  internalAccounts_.clear();
  internalAccounts_.reserve(count);
  accountKeys_.assign(count, entity::Key{});

  protectionType_.assign(count, ProtectionType::none);
  bankTier_.assign(count, BankTier::zeroFee);
  overdraftFeeAmount_.assign(count, 0.0);

  locTracker_.initialize(count);

  emitLiquidity_ = true;
  liquiditySink_ = nullptr;
}

void Ledger::addAccount(const entity::Key &id, Index idx) {
  assert(idx < size_);
  internalAccounts_.insert_or_assign(id, idx);
  accountKeys_[idx] = id;
}

void Ledger::createHub(Index idx) noexcept {
  assert(idx < size_);
  flags_[idx] = static_cast<std::uint8_t>(flags_[idx] | hub);
}

bool Ledger::isHub(Index idx) const noexcept {
  assert(idx < size_);
  return (flags_[idx] & hub) != 0;
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
  if (isHub(idx)) {
    return std::numeric_limits<double>::infinity();
  }
  return totalLiquidity(idx);
}

double Ledger::availableCash(Index idx) const noexcept {
  if (!isValid(idx)) {
    return 0.0;
  }
  if (isHub(idx)) {
    return std::numeric_limits<double>::infinity();
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
    // LOC credit limit lives in the overdraft slot so totalLiquidity()
    // and the funding check see it through the existing code path.
    // APR + billingDay arrive later via setLoc().
    overdrafts_[idx] = bufferAmount;
    // Register with the tracker at zero APR/day 0; setLoc() fills in
    // the real values. This keeps the tracker's enabled-flag in sync
    // with the protection type even if the caller forgets setLoc.
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

TransferDecision Ledger::applyTransfer(Index srcIdx, Index dstIdx,
                                       double amount, channels::Tag channel,
                                       double &srcCashBefore) noexcept {
  srcCashBefore = 0.0;

  if (amount <= 0.0 || !std::isfinite(amount)) {
    return TransferDecision::reject(RejectReason::invalid);
  }

  const bool srcExternal = (srcIdx == invalid);
  const bool dstExternal = (dstIdx == invalid);

  if (srcExternal && dstExternal) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  if (srcExternal) {
    cash_[dstIdx] += amount;
    return TransferDecision::accept();
  }

  const bool srcHub = isHub(srcIdx);
  srcCashBefore = cash_[srcIdx];

  if (!srcHub && !channels::isLiquidity(channel)) {
    const bool selfTransfer =
        channels::is(channel, channels::Legit::selfTransfer);
    const double spendable =
        selfTransfer ? cash_[srcIdx] : totalLiquidity(srcIdx);
    if (spendable < amount) {
      return TransferDecision::reject(RejectReason::unfunded);
    }
  }

  if (dstExternal) {
    if (!srcHub) {
      cash_[srcIdx] -= amount;
    }
    return TransferDecision::accept();
  }

  if (!srcHub) {
    cash_[srcIdx] -= amount;
  }
  cash_[dstIdx] += amount;
  return TransferDecision::accept();
}

TransferDecision Ledger::transfer(Index srcIdx, Index dstIdx, double amount,
                                  channels::Tag channel) noexcept {
  double srcCashBefore = 0.0;
  return applyTransfer(srcIdx, dstIdx, amount, channel, srcCashBefore);
}

TransferDecision Ledger::transfer(const entity::Key &src,
                                  const entity::Key &dst, double amount,
                                  channels::Tag channel) {
  const bool srcExternal = isExternalAccount(src);
  const bool dstExternal = isExternalAccount(dst);

  const Index srcIdx = srcExternal ? invalid : findAccount(src);
  const Index dstIdx = dstExternal ? invalid : findAccount(dst);

  if (!srcExternal && srcIdx == invalid) {
    return TransferDecision::reject(RejectReason::unbooked);
  }
  if (!dstExternal && dstIdx == invalid) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  return transfer(srcIdx, dstIdx, amount, channel);
}

void Ledger::debitAndEmit(Index idx, double amount, channels::Tag channel,
                          std::int64_t timestamp) {
  // Bypass the funding check: fee collection / interest must apply
  // against already-negative balances.
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

TransferDecision Ledger::transferAt(Index srcIdx, Index dstIdx, double amount,
                                    channels::Tag channel,
                                    std::int64_t timestamp) noexcept {
  // Roll LOC integrals forward on both legs using pre-transfer cash.
  if (isValid(srcIdx)) {
    locTracker_.update(srcIdx, cash_[srcIdx], timestamp);
  }
  if (isValid(dstIdx)) {
    locTracker_.update(dstIdx, cash_[dstIdx], timestamp);
  }

  double srcCashBefore = 0.0;
  const auto decision =
      applyTransfer(srcIdx, dstIdx, amount, channel, srcCashBefore);

  if (decision.rejected() || !emitLiquidity_ || srcIdx == invalid) {
    return decision;
  }

  const auto fee =
      assessOverdraftFee(protectionType_[srcIdx], channel, srcCashBefore,
                         cash_[srcIdx], overdraftFeeAmount_[srcIdx]);
  if (fee) {
    debitAndEmit(srcIdx, fee.amount,
                 channels::tag(channels::Liquidity::overdraftFee), timestamp);
  }

  return decision;
}

void Ledger::accrueLocInterestThrough(std::int64_t timestamp) noexcept {
  // Pull matured accruals out of the tracker, then turn each into a
  // debit + emission here. Keeping the tracker ignorant of Ledger /
  // LiquiditySink means the tracker stays independently testable.
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

  // LOC tracker integrals + timestamps must be restored so the next
  // replay starts from the same integration state. The prior
  // implementation only copied three vectors and silently drifted if
  // the billing clock had already been anchored in the first pass.
  locTracker_.copyStateFrom(other.locTracker_);
}

} // namespace PhantomLedger::clearing
