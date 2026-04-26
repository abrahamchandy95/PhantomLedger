#pragma once

#include "phantomledger/primitives/concurrent/account_lock_array.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <cstddef>

namespace PhantomLedger::spending::simulator {

class ParallelLedgerView {
public:
  ParallelLedgerView() = default;

  ParallelLedgerView(
      ::PhantomLedger::clearing::Ledger *ledger,
      primitives::concurrent::AccountLockArray *lockArray) noexcept
      : ledger_(ledger), lockArray_(lockArray) {}

  [[nodiscard]] bool empty() const noexcept { return ledger_ == nullptr; }

  [[nodiscard]] ::PhantomLedger::clearing::TransferDecision
  transfer(::PhantomLedger::clearing::Ledger::Index srcIdx,
           ::PhantomLedger::clearing::Ledger::Index dstIdx, double amount,
           ::PhantomLedger::channels::Tag channel) noexcept {
    if (ledger_ == nullptr) {
      return ::PhantomLedger::clearing::TransferDecision::accept();
    }

    if (lockArray_ == nullptr) {
      // Serial mode — same call the Phase 2 code makes.
      return ledger_->transfer(srcIdx, dstIdx, amount, channel);
    }

    // Parallel mode — lock the relevant slot(s), transfer, unlock.
    constexpr auto kInvalid = ::PhantomLedger::clearing::Ledger::invalid;
    lockArray_->lockPair(static_cast<std::size_t>(srcIdx),
                         static_cast<std::size_t>(dstIdx),
                         static_cast<std::size_t>(kInvalid));

    auto decision = ledger_->transfer(srcIdx, dstIdx, amount, channel);

    lockArray_->unlockPair(static_cast<std::size_t>(srcIdx),
                           static_cast<std::size_t>(dstIdx),
                           static_cast<std::size_t>(kInvalid));
    return decision;
  }

  [[nodiscard]] double
  availableCash(::PhantomLedger::clearing::Ledger::Index idx) const noexcept {
    if (ledger_ == nullptr ||
        idx == ::PhantomLedger::clearing::Ledger::invalid) {
      return 0.0;
    }
    return ledger_->availableCash(idx);
  }

  [[nodiscard]] ::PhantomLedger::clearing::Ledger *ledger() const noexcept {
    return ledger_;
  }

private:
  ::PhantomLedger::clearing::Ledger *ledger_ = nullptr;
  primitives::concurrent::AccountLockArray *lockArray_ = nullptr;
};

} // namespace PhantomLedger::spending::simulator
