#pragma once
/*
 * Insurance claim emitter.
 *
 * For each insured person, runs one Bernoulli draw per coverage type
 * (auto, home) using a window-scaled probability derived from the
 * per-policy `annualClaimP`. When a claim fires, the payout amount is
 * sampled from the lognormal distribution carried in `ClaimRates`.
 */

#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/insurance/premiums.hpp"
#include "phantomledger/transfers/insurance/rates.hpp"

#include <vector>

namespace PhantomLedger::transfers::insurance {

/// Emit claim transactions across the window. Sorted by timestamp on
/// return.
[[nodiscard]] std::vector<transactions::Transaction>
claims(const ClaimRates &rates, const time::Window &window, random::Rng &rng,
       const transactions::Factory &txf, const random::RngFactory &factory,
       const entity::product::PortfolioRegistry &portfolios,
       const Population &population);

} // namespace PhantomLedger::transfers::insurance
