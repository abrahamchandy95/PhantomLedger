#pragma once

#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/math/evolution.hpp"
#include "phantomledger/primitives/random/factory.hpp"

#include <span>

namespace PhantomLedger::activity::spending::dynamics::monthly {

/* `ts` is the month-boundary instant, in epoch seconds. It drives the
 * merchant-liveness rebuild: the national CDF and every distance-decay pool
 * are recomputed over the merchants open on that date, so a closed merchant
 * stops receiving new transactions and a newly opened one becomes reachable.
 *
 * `homeAreas` is index-aligned to personIdx and carries the CURRENT home of
 * each person, so the favourite ADD pass draws from the same
 * home-conditioned membership law the bootstrap draw uses. An empty span
 * degrades to the geography-free reach CDF, which is what a caller with no
 * geography gets.
 *
 * EVERY DRAW IS ON A LANE OF `lanes`, ONE PER PASS PER PERSON PER MONTH
 * (evolver-lanes-2026-09): {"evolve-contacts" | "evolve-billers" |
 * "evolve-favourites", PersonId, "YYYY-MM" of `ts`}. All three passes retry,
 * so their draw counts depend on the contact graph and on the biller and
 * favourite sets. They used to share the caller's session rng, which then
 * drew every day frame and population-dynamics multiplier, so any change to
 * a biller or favourite set moved every later day of the fold. Each lane now
 * carries one data-dependent draw sequence and nothing after it
 * (merchant-churn-2026-07 rule 2). The key is the calendar month, not the
 * day index, so it does not depend on where the window starts. */
void evolveAll(const random::RngFactory &lanes,
               const math::evolution::Config &cfg,
               market::commerce::View &commerce, std::uint32_t totalPersons,
               std::int64_t ts,
               std::span<const entity::geography::GeoAreaId> homeAreas = {});

} // namespace PhantomLedger::activity::spending::dynamics::monthly
