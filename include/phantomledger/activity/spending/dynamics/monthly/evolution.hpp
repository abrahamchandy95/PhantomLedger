#pragma once

#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/math/evolution.hpp"
#include "phantomledger/primitives/random/rng.hpp"

namespace PhantomLedger::spending::dynamics::monthly {

void evolveAll(random::Rng &rng, const math::evolution::Config &cfg,
               market::commerce::View &commerce, std::uint32_t totalPersons);

} // namespace PhantomLedger::spending::dynamics::monthly
