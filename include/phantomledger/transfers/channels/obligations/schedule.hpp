#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/loan_terms_ledger.hpp"
#include "phantomledger/entities/products/obligation_stream.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::obligations {

struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;
};

class Scheduler {
public:
  Scheduler(random::Rng &rng, const transactions::Factory &txf) noexcept;

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const entity::product::LoanTermsLedger &loans,
           const entity::product::ObligationStream &obligations,
           time::HalfOpenInterval active, const Population &population) const;

private:
  random::Rng *rng_ = nullptr;
  const transactions::Factory *txf_ = nullptr;
};

} // namespace PhantomLedger::transfers::obligations
