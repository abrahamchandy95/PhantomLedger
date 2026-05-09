#pragma once

#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/dispute_behavior.hpp"

#include <optional>

namespace PhantomLedger::transfers::credit_cards {

class DisputeSampler {
public:
  DisputeSampler(const DisputeBehavior &behavior,
                 const transactions::Factory &factory) noexcept
      : behavior_(behavior), factory_(factory) {}

  [[nodiscard]] std::optional<transactions::Transaction>
  sample(random::Rng &rng, const transactions::Transaction &purchase,
         time::TimePoint windowEndExcl) const;

private:
  [[nodiscard]] std::optional<transactions::Transaction>
  buildCredit(random::Rng &rng, const transactions::Transaction &purchase,
              channels::Credit kind, time::TimePoint windowEndExcl) const;

  const DisputeBehavior &behavior_;
  const transactions::Factory &factory_;
};

} // namespace PhantomLedger::transfers::credit_cards
