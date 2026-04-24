#include "phantomledger/transactions/clearing/ledger.hpp"

#include "phantomledger/taxonomies/channels/predicates.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

namespace PhantomLedger::clearing {

namespace {

[[nodiscard]] constexpr bool
isExternalAccount(const entities::identifier::Key &id) noexcept {
  return id.bank == entities::identifier::Bank::external;
}

// Seconds in a 365.25-day year. Used to convert APR * dollar-seconds
// into a dollar interest amount.
constexpr double kYearSeconds = 365.25 * 86400.0;

// Default LOC billing period. Matches a typical monthly statement
// cycle. Accounts become eligible for interest emission once this
// many seconds have elapsed since the previous billing.
constexpr std::int64_t kLocBillingPeriodSeconds = 30 * 86400;

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
  accountKeys_.assign(count, entities::identifier::Key{});

  protectionType_.assign(count, ProtectionType::none);
  bankTier_.assign(count, BankTier::zeroFee);
  overdraftFeeAmount_.assign(count, 0.0);

  locApr_.assign(count, 0.0);
  locBillingDay_.assign(count, 0);
  locDollarSecondsIntegral_.assign(count, 0.0);
  locLastUpdateTs_.assign(count, 0);
  locLastBillingTs_.assign(count, 0);

  emitLiquidity_ = true;
  liquiditySink_ = nullptr;
}

void Ledger::addAccount(const entities::identifier::Key &id, Index idx) {
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

double Ledger::liquidity(const entities::identifier::Key &identity) const {
  return liquidity(findAccount(identity));
}

double Ledger::availableCash(const entities::identifier::Key &identity) const {
  return availableCash(findAccount(identity));
}

Ledger::Index
Ledger::findAccount(const entities::identifier::Key &identity) const {
  const auto it = internalAccounts_.find(identity);
  return it == internalAccounts_.end() ? invalid : it->second;
}

void Ledger::setOverdraftOnly(Index idx, double limit) noexcept {
  assert(idx < size_);
  cash_[idx] = 0.0;
  overdrafts_[idx] = limit;
  linked_[idx] = 0.0;
  courtesy_[idx] = 0.0;
  protectionType_[idx] =
      limit > 0.0 ? ProtectionType::courtesy : ProtectionType::none;
}

// --- Protection / tier / LOC setters ---

void Ledger::setProtection(Index idx, ProtectionType type,
                           double bufferAmount) noexcept {
  assert(idx < size_);

  // Clear all buffer slots to enforce mutual exclusivity, then write
  // the new buffer into the slot that matches the protection type.
  overdrafts_[idx] = 0.0;
  linked_[idx] = 0.0;
  courtesy_[idx] = 0.0;

  protectionType_[idx] = type;

  switch (type) {
  case ProtectionType::none:
    break;
  case ProtectionType::courtesy:
    courtesy_[idx] = bufferAmount;
    break;
  case ProtectionType::linked:
    linked_[idx] = bufferAmount;
    break;
  case ProtectionType::loc:
    // LOC credit limit lives in the overdraft slot so it feeds
    // totalLiquidity() and the funding check through the existing
    // code path without a special case. The APR / billingDay live
    // in the dedicated LOC vectors and are set via setLoc().
    overdrafts_[idx] = bufferAmount;
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
  locApr_[idx] = apr;
  locBillingDay_[idx] = static_cast<std::int32_t>(billingDay);
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

// --- Liquidity sink & mode ---

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

  // External source -> internal destination: credit only.
  if (srcExternal) {
    cash_[dstIdx] += amount;
    return TransferDecision::accept();
  }

  const bool srcHub = isHub(srcIdx);
  srcCashBefore = cash_[srcIdx];

  // Funding check. Bypassed for hubs (infinite cash) and for channels
  // in the Liquidity block (ledger-emitted fee/interest transfers
  // that must be applicable against already-overdrawn accounts).
  if (!srcHub && !channels::isLiquidity(channel)) {
    const bool selfTransfer =
        channels::is(channel, channels::Legit::selfTransfer);
    const double spendable =
        selfTransfer ? cash_[srcIdx] : totalLiquidity(srcIdx);
    if (spendable < amount) {
      return TransferDecision::reject(RejectReason::unfunded);
    }
  }

  // Internal source -> external destination: debit only.
  if (dstExternal) {
    if (!srcHub) {
      cash_[srcIdx] -= amount;
    }
    return TransferDecision::accept();
  }

  // Both internal.
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

TransferDecision Ledger::transfer(const entities::identifier::Key &src,
                                  const entities::identifier::Key &dst,
                                  double amount, channels::Tag channel) {
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

// --- Full-replay transfer ---

void Ledger::updateLocAccrual(Index idx, std::int64_t timestamp) noexcept {
  if (protectionType_[idx] != ProtectionType::loc) {
    return;
  }

  const auto lastTs = locLastUpdateTs_[idx];
  if (lastTs != 0 && timestamp > lastTs && cash_[idx] < 0.0) {
    const double elapsed = static_cast<double>(timestamp - lastTs);
    locDollarSecondsIntegral_[idx] += (-cash_[idx]) * elapsed;
  }
  locLastUpdateTs_[idx] = timestamp;
}

void Ledger::emitOverdraftFee(Index idx, std::int64_t timestamp) {
  const double fee = overdraftFeeAmount_[idx];
  if (fee <= 0.0) {
    return;
  }

  // Bypass funding check: fee collection must apply even when the
  // account is already overdrawn.
  cash_[idx] -= fee;

  if (liquiditySink_) {
    liquiditySink_(LiquidityEvent{
        .channel = channels::tag(channels::Liquidity::overdraftFee),
        .payerKey = accountKeys_[idx],
        .amount = fee,
        .timestamp = timestamp,
    });
  }
}

TransferDecision Ledger::transferAt(Index srcIdx, Index dstIdx, double amount,
                                    channels::Tag channel,
                                    std::int64_t timestamp) noexcept {
  // Accrue LOC dollar-seconds on both legs up to `timestamp` BEFORE
  // applying the transfer, so the integral reflects the balance
  // during the pre-transfer period. The post-transfer balance is
  // captured on the next call touching this account.
  if (isValid(srcIdx)) {
    updateLocAccrual(srcIdx, timestamp);
  }
  if (isValid(dstIdx)) {
    updateLocAccrual(dstIdx, timestamp);
  }

  double srcCashBefore = 0.0;
  const auto decision =
      applyTransfer(srcIdx, dstIdx, amount, channel, srcCashBefore);

  if (decision.rejected()) {
    return decision;
  }

  // Overdraft-crossing detection. Only meaningful for an internal
  // source account with COURTESY or LINKED protection: LOC uses
  // interest accrual instead, NONE has no fee infrastructure, and
  // external sources can't overdraft.
  if (!emitLiquidity_ || srcIdx == invalid) {
    return decision;
  }

  const auto protection = protectionType_[srcIdx];
  if (protection != ProtectionType::courtesy &&
      protection != ProtectionType::linked) {
    return decision;
  }

  // Don't recursively fee a liquidity-event transfer.
  if (channels::isLiquidity(channel)) {
    return decision;
  }

  const bool crossedIntoNegative =
      (srcCashBefore >= 0.0) && (cash_[srcIdx] < 0.0);
  if (crossedIntoNegative) {
    emitOverdraftFee(srcIdx, timestamp);
  }

  return decision;
}

void Ledger::accrueLocInterestThrough(std::int64_t timestamp) noexcept {
  for (Index idx = 0; idx < size_; ++idx) {
    if (protectionType_[idx] != ProtectionType::loc) {
      continue;
    }

    // Roll the integral forward to `timestamp` so any interest we
    // emit reflects debit balance held right up to this moment.
    updateLocAccrual(idx, timestamp);

    const auto lastBilling = locLastBillingTs_[idx];
    if (lastBilling == 0) {
      // First sweep for this account: anchor the billing clock and
      // let interest start accruing from here.
      locLastBillingTs_[idx] = timestamp;
      continue;
    }

    if (timestamp - lastBilling < kLocBillingPeriodSeconds) {
      continue;
    }

    const double interest =
        locDollarSecondsIntegral_[idx] * locApr_[idx] / kYearSeconds;

    if (interest > 0.0 && emitLiquidity_) {
      // Bypass funding check: LOC interest must apply against a
      // balance that is presumably already negative.
      cash_[idx] -= interest;

      if (liquiditySink_) {
        liquiditySink_(LiquidityEvent{
            .channel = channels::tag(channels::Liquidity::locInterest),
            .payerKey = accountKeys_[idx],
            .amount = interest,
            .timestamp = timestamp,
        });
      }
    }

    locDollarSecondsIntegral_[idx] = 0.0;
    locLastBillingTs_[idx] = timestamp;
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

  // LOC accrual state also needs to be restored so the next replay
  // starts from the same dollar-seconds integral.
  std::copy(other.locDollarSecondsIntegral_.begin(),
            other.locDollarSecondsIntegral_.end(),
            locDollarSecondsIntegral_.begin());
  std::copy(other.locLastUpdateTs_.begin(), other.locLastUpdateTs_.end(),
            locLastUpdateTs_.begin());
  std::copy(other.locLastBillingTs_.begin(), other.locLastBillingTs_.end(),
            locLastBillingTs_.begin());
}

} // namespace PhantomLedger::clearing
