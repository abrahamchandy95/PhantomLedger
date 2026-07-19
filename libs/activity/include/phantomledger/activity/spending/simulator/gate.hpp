#pragma once

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

namespace PhantomLedger::activity::spending::simulator {

class Gate {
public:
  using Ledger = ::PhantomLedger::clearing::Ledger;
  using Index = Ledger::Index;
  using Decision = ::PhantomLedger::clearing::TransferDecision;
  using Channel = ::PhantomLedger::channels::Tag;

  Gate() = default;

  explicit Gate(const Ledger *ledger) noexcept : ledger_(ledger) {}

  [[nodiscard]] bool attached() const noexcept { return ledger_ != nullptr; }

  [[nodiscard]] Decision decide(Index srcIdx, Index dstIdx, double amount,
                                Channel channel) const noexcept {
    if (ledger_ == nullptr) {
      return Decision::accept();
    }
    return ledger_->decide(srcIdx, dstIdx, amount, channel);
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

  const Ledger *ledger_ = nullptr;
};

} // namespace PhantomLedger::activity::spending::simulator
