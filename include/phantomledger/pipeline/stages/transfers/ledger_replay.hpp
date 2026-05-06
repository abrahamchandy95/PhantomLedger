#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <memory>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

class LedgerReplay {
public:
  using Transaction = ::PhantomLedger::transactions::Transaction;
  using FundingBehavior =
      ::PhantomLedger::transfers::legit::ledger::ReplayFundingBehavior;

  struct Ordering {
    FundingBehavior funding{};
  };

  struct Candidate {
    std::vector<Transaction> txns;
    ::PhantomLedger::pipeline::ReplayDrops drops;
  };

  struct Posted {
    std::vector<Transaction> txns;
    std::unique_ptr<::PhantomLedger::clearing::Ledger> book;
  };

  LedgerReplay() = default;

  LedgerReplay &ordering(Ordering value) noexcept;
  LedgerReplay &fundingBehavior(FundingBehavior value) noexcept;

  [[nodiscard]] Candidate
  preFraud(const ::PhantomLedger::clearing::Ledger &initialBook,
           ::PhantomLedger::random::Rng &rng,
           std::vector<Transaction> sorted) const;

  [[nodiscard]] Posted
  postFraud(::PhantomLedger::random::Rng &rng,
            const ::PhantomLedger::clearing::Ledger &initialBook,
            std::vector<Transaction> merged) const;

private:
  Ordering ordering_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
