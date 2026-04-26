#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/credit_cards/policy/behavior.hpp"
#include "phantomledger/transfers/credit_cards/policy/issuer.hpp"

#include <optional>

namespace PhantomLedger::transfers::credit_cards {

[[nodiscard]] std::optional<transactions::Transaction> sampleMerchantCredit(
    const DisputeWindow &window, const DisputeRates &rates, random::Rng &rng,
    const transactions::Transaction &purchase, time::TimePoint windowEndExcl,
    const transactions::Factory &factory);

} // namespace PhantomLedger::transfers::credit_cards
