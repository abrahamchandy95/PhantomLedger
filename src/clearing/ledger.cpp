#include "phantomledger/clearing/ledger.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace PhantomLedger::clearing {

namespace {

[[nodiscard]] constexpr bool
isExternalAccount(const entities::identifier::Key &id) noexcept {
  return id.bank == entities::identifier::Bank::external;
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
}

void Ledger::addAccount(const entities::identifier::Key &id, Index idx) {
  assert(idx < size_);
  internalAccounts_.insert_or_assign(id, idx);
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
}

TransferDecision Ledger::transfer(const entities::identifier::Key &src,
                                  const entities::identifier::Key &dst,
                                  double amount, channels::Tag channel) {
  if (amount <= 0.0 || !std::isfinite(amount)) {
    return TransferDecision::reject(RejectReason::invalid);
  }

  const bool srcExternal = isExternalAccount(src);
  const bool dstExternal = isExternalAccount(dst);

  const Index srcIdx = srcExternal ? invalid : findAccount(src);
  const Index dstIdx = dstExternal ? invalid : findAccount(dst);

  if (!srcExternal && !isValid(srcIdx)) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  if (!dstExternal && !isValid(dstIdx)) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  if (srcExternal && dstExternal) {
    return TransferDecision::reject(RejectReason::unbooked);
  }

  if (srcExternal) {
    cash_[dstIdx] += amount;
    return TransferDecision::accept();
  }

  const bool srcHub = isHub(srcIdx);

  if (!srcHub) {
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

Ledger Ledger::clone() const { return *this; }

void Ledger::restore(const Ledger &other) {
  assert(size_ == other.size_);

  std::copy(other.cash_.begin(), other.cash_.end(), cash_.begin());
  std::copy(other.overdrafts_.begin(), other.overdrafts_.end(),
            overdrafts_.begin());
  std::copy(other.linked_.begin(), other.linked_.end(), linked_.begin());
  std::copy(other.courtesy_.begin(), other.courtesy_.end(), courtesy_.begin());
}

} // namespace PhantomLedger::clearing
