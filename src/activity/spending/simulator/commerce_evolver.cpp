#include "phantomledger/activity/spending/simulator/commerce_evolver.hpp"

#include "phantomledger/activity/spending/dynamics/monthly/evolution.hpp"
#include "phantomledger/primitives/time/calendar.hpp"

namespace PhantomLedger::activity::spending::simulator {
namespace {

[[nodiscard]] bool isMonthBoundary(std::uint32_t dayIndex,
                                   time::TimePoint windowStart) noexcept {
  if (dayIndex == 0) {
    return false;
  }

  const auto prev = time::addDays(windowStart, static_cast<int>(dayIndex) - 1);
  const auto curr = time::addDays(windowStart, static_cast<int>(dayIndex));

  const auto prevCal = time::toCalendarDate(prev);
  const auto currCal = time::toCalendarDate(curr);

  return currCal.month != prevCal.month || currCal.year != prevCal.year;
}

} // namespace

CommerceEvolver::CommerceEvolver(math::evolution::Config config)
    : config_(config) {}

void CommerceEvolver::evolveIfNeeded(market::Market &market, random::Rng &rng,
                                     std::uint32_t dayIndex) const {
  if (!isMonthBoundary(dayIndex, market.bounds().startDate)) {
    return;
  }

  auto &commerce = market.commerceMutable();

  // The month-boundary instant the liveness rebuild is evaluated at.
  const auto boundary = time::toEpochSeconds(
      time::addDays(market.bounds().startDate, static_cast<int>(dayIndex)));

  // relocation-2026-07: HOMES MOVE FIRST, and the order is load-bearing.
  // `evolveAll` rebuilds the live geo pools and the favourite/biller CDFs for
  // this month; if homes were refreshed after that, a household that moved
  // this month would spend the month selecting against its OLD area's pool.
  // Draw-free, so the ordering changes no stream position.
  market.populationMutable().refreshHomes(boundary);

  dynamics::monthly::evolveAll(rng, config_, commerce,
                               market.population().count(), boundary,
                               market.population().homeAreas());
}

} // namespace PhantomLedger::activity::spending::simulator
