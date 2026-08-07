#pragma once

#include "phantomledger/pipeline/chunk/schedule.hpp"
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
    /* Authorizations the funding test declined. Carried out of the replay
     * rather than counted and dropped — see `ledger::DeclinedAttempt`. */
    std::vector<::PhantomLedger::transfers::legit::ledger::DeclinedAttempt>
        declined;
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

  [[nodiscard]] Candidate
  preFraudChunked(const clearing::Ledger &initialBook, random::Rng &rng,
                  std::vector<Transaction> sorted,
                  const pipeline::chunk::Schedule &schedule) const;

  [[nodiscard]] Posted
  postFraudChunked(random::Rng &rng, const clearing::Ledger &initialBook,
                   std::vector<Transaction> merged,
                   const pipeline::chunk::Schedule &schedule) const;

  [[nodiscard]] Posted
  postFraudChunkedMerged(random::Rng &rng, const clearing::Ledger &initialBook,
                         std::vector<Transaction> candidatesSorted,
                         std::vector<Transaction> fraud,
                         const pipeline::chunk::Schedule &schedule) const;

private:
  Ordering ordering_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
