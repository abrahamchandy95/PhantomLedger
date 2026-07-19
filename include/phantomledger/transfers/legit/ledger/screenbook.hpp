#pragma once

#include "phantomledger/transactions/clearing/ledger.hpp"

#include <memory>

namespace PhantomLedger::transfers::legit::ledger {

class ScreenBook {
public:
  ScreenBook() = default;

  explicit ScreenBook(const clearing::Ledger *initialBook) noexcept
      : initialBook_(initialBook) {}

  [[nodiscard]] clearing::Ledger *fresh() {
    if (initialBook_ == nullptr) {
      return nullptr;
    }
    if (scratch_ == nullptr) {
      scratch_ = std::make_unique<clearing::Ledger>(initialBook_->clone());
      return scratch_.get();
    }
    scratch_->restore(*initialBook_);
    return scratch_.get();
  }

  [[nodiscard]] bool hasInitialBook() const noexcept {
    return initialBook_ != nullptr;
  }

private:
  const clearing::Ledger *initialBook_ = nullptr;
  std::unique_ptr<clearing::Ledger> scratch_{};
};

} // namespace PhantomLedger::transfers::legit::ledger
