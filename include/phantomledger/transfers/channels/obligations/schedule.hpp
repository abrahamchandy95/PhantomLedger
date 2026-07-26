#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/loan_terms_ledger.hpp"
#include "phantomledger/entities/products/obligation_stream.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::obligations {

struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;

  // H3 part 3c-ii (authority U-8 addendum): the persona-timeline
  // carrier (PersonId-1 indexed). Loan/tax obligation events stop
  // posting at ACCOUNT CLOSURE (death + settlement — the estate
  // services them until then) via an emission-side filter AFTER each
  // event's draws burn, so the shared rng stream is byte-identical.
  // Empty (the default) stands the filter down.
  std::span<const synth::personas::timeline::Timeline> timelines{};
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
