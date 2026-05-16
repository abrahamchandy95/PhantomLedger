#pragma once

#include "phantomledger/pipeline/transfers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <memory>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

class LedgerReplay {
public:
  using Transaction = transactions::Transaction;
  using FundingBehavior =
      ::PhantomLedger::transfers::legit::ledger::ReplayFundingBehavior;

  struct Ordering {
    FundingBehavior funding{};
  };

  struct Candidate {
    std::vector<Transaction> txns;
    ReplayDrops drops;
  };

  struct Posted {
    std::vector<Transaction> txns;
    std::unique_ptr<clearing::Ledger> book;
  };

  LedgerReplay() = default;

  LedgerReplay &ordering(Ordering value) noexcept;
  LedgerReplay &fundingBehavior(FundingBehavior value) noexcept;

  [[nodiscard]] Candidate preFraud(const clearing::Ledger &initialBook,
                                   random::Rng &rng,
                                   std::vector<Transaction> sorted) const;

  [[nodiscard]] Posted postFraud(random::Rng &rng,
                                 const clearing::Ledger &initialBook,
                                 std::vector<Transaction> merged) const;

private:
  Ordering ordering_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
