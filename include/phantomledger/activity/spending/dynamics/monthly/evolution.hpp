#pragma once

#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/math/evolution.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <span>

namespace PhantomLedger::activity::spending::dynamics::monthly {

// `ts` is the month-boundary instant, in epoch seconds. It drives the
// merchant-liveness rebuild (merchant-churn-2026-07): the national CDF and
// every distance-decay pool are recomputed over the merchants open on that
// date, so a closed merchant stops receiving new transactions and a newly
// opened one becomes reachable.
// merchant-selection-2026-08: `homeAreas` is index-aligned to personIdx and
// carries the CURRENT home of each person, so the favourite ADD pass can draw
// from the same home-conditioned membership law the bootstrap draw uses. An
// empty span degrades to the geography-free reach CDF — which is what every
// caller predating the geo model gets, and it is the pre-round behaviour.
void evolveAll(random::Rng &rng, const math::evolution::Config &cfg,
               market::commerce::View &commerce, std::uint32_t totalPersons,
               std::int64_t ts,
               std::span<const entity::geography::GeoAreaId> homeAreas = {});

} // namespace PhantomLedger::activity::spending::dynamics::monthly
