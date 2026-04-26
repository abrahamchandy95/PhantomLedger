#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transfers/credit_cards/policy/behavior.hpp"

namespace PhantomLedger::transfers::credit_cards {

[[nodiscard]] double samplePaymentAmount(const PaymentMixture &mixture,
                                         random::Rng &rng, double statementAbs,
                                         double minimumDue);

[[nodiscard]] time::TimePoint samplePaymentTime(const PaymentTiming &timing,
                                                random::Rng &rng,
                                                time::TimePoint due,
                                                bool autopay);

} // namespace PhantomLedger::transfers::credit_cards
