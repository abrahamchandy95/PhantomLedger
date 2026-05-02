#include "phantomledger/spending/simulator/commerce_evolver.hpp"

#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/spending/dynamics/monthly/evolution.hpp"

namespace PhantomLedger::spending::simulator {
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

  dynamics::monthly::evolveAll(
      rng, config_, commerce, commerce.merchCdf(),
      static_cast<std::uint32_t>(commerce.merchCdf().size()),
      market.population().count());
}

} // namespace PhantomLedger::spending::simulator
