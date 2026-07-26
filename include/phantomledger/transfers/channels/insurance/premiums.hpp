#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/products/insurance_ledger.hpp"
#include "phantomledger/entities/products/loan_terms_ledger.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::transfers::insurance {

struct Population {
  const std::unordered_map<entity::PersonId, entity::Key> *primaryAccounts =
      nullptr;

  // H3 part 3c-ii (authority U-8 addendum): the persona-timeline
  // carrier (PersonId-1 indexed). Premiums stop at ACCOUNT CLOSURE
  // (death + settlement — the estate services them until then);
  // claims stop at DEATH (claim filing is behavioral). Both are
  // emission-side filters AFTER the sites' draws burn, so the shared
  // rng streams are byte-identical. Empty (the default) stands the
  // filters down.
  std::span<const synth::personas::timeline::Timeline> timelines{};
};

class PremiumGenerator {
public:
  PremiumGenerator(random::Rng &rng, const transactions::Factory &txf) noexcept
      : rng_(&rng), txf_(&txf) {}

  [[nodiscard]] std::vector<transactions::Transaction>
  generate(const time::Window &window,
           const entity::product::InsuranceLedger &insurance,
           const entity::product::LoanTermsLedger &loans,
           const Population &population) const;

private:
  random::Rng *rng_;
  const transactions::Factory *txf_;
};

} // namespace PhantomLedger::transfers::insurance
