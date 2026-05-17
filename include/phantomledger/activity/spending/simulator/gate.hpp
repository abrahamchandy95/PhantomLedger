#pragma once

#include "phantomledger/primitives/concurrent/account_lock_array.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <cstddef>

namespace PhantomLedger::activity::spending::simulator {

class Gate {
public:
  using Ledger = ::PhantomLedger::clearing::Ledger;
  using Index = Ledger::Index;
  using Decision = ::PhantomLedger::clearing::TransferDecision;
  using Channel = ::PhantomLedger::channels::Tag;
  using LockArray = ::PhantomLedger::primitives::concurrent::AccountLockArray;

  Gate() = default;

  Gate(Ledger *ledger, LockArray *lockArray) noexcept
      : ledger_(ledger), lockArray_(lockArray) {}

  [[nodiscard]] bool attached() const noexcept { return ledger_ != nullptr; }
  [[nodiscard]] Ledger *ledger() const noexcept { return ledger_; }

  [[nodiscard]] Decision transfer(Index srcIdx, Index dstIdx, double amount,
                                  Channel channel) noexcept {
    if (ledger_ == nullptr) {
      return Decision::accept();
    }
    if (lockArray_ == nullptr) {
      return ledger_->transfer(srcIdx, dstIdx, amount, channel);
    }

    const auto src = static_cast<std::size_t>(srcIdx);
    const auto dst = static_cast<std::size_t>(dstIdx);
    constexpr auto inv = static_cast<std::size_t>(Ledger::invalid);

    lockArray_->lockPair(src, dst, inv);
    const auto decision = ledger_->transfer(srcIdx, dstIdx, amount, channel);
    lockArray_->unlockPair(src, dst, inv);
    return decision;
  }

  [[nodiscard]] double availableCash(Index idx) const noexcept {
    return canRead(idx) ? ledger_->availableCash(idx) : 0.0;
  }

  [[nodiscard]] double liquidity(Index idx) const noexcept {
    return canRead(idx) ? ledger_->liquidity(idx) : 0.0;
  }

private:
  [[nodiscard]] bool canRead(Index idx) const noexcept {
    return ledger_ != nullptr && idx != Ledger::invalid;
  }

  Ledger *ledger_ = nullptr;
  LockArray *lockArray_ = nullptr;
};

} // namespace PhantomLedger::activity::spending::simulator
