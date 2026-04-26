#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/math/evolution.hpp"
#include "phantomledger/spending/market/commerce/view.hpp"

#include <span>

namespace PhantomLedger::spending::dynamics::monthly {

void evolveAll(random::Rng &rng, const math::evolution::Config &cfg,
               market::commerce::View &commerce,
               std::span<const double> globalMerchCdf,
               std::uint32_t totalMerchants, std::uint32_t totalPersons);

} // namespace PhantomLedger::spending::dynamics::monthly
