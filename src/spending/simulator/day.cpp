#include "phantomledger/spending/simulator/driver.hpp"

#include "phantomledger/math/counts.hpp"
#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/spending/dynamics/monthly/evolution.hpp"
#include "phantomledger/spending/simulator/loop.hpp"
#include "phantomledger/spending/simulator/thread_runner.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

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

void Simulator::runDay(const RunPlan &plan, RunState &state,
                       std::uint32_t dayIndex) {
  evolveCommerceIfNeeded(dayIndex);

  const auto frame = buildDayFrame(plan, dayIndex);

  advanceLedgerToDay(plan, state, frame);

  const auto paydayPersons = updatePaydayState(plan, state, dayIndex);

  advanceDynamics(plan, paydayPersons);

  draftSpenderTransactions(plan, state, frame);
}

void Simulator::evolveCommerceIfNeeded(std::uint32_t dayIndex) {
  if (!isMonthBoundary(dayIndex, market_.bounds().startDate)) {
    return;
  }

  auto &mutCommerce = const_cast<market::Market &>(market_).commerceMutable();

  dynamics::monthly::evolveAll(
      *engine_.rng, config_.dynamics.evolution, mutCommerce,
      mutCommerce.merchCdf(),
      static_cast<std::uint32_t>(mutCommerce.merchCdf().size()),
      market_.population().count());
}

actors::DayFrame Simulator::buildDayFrame(const RunPlan &plan,
                                          std::uint32_t dayIndex) {
  const auto day =
      actors::buildDay(market_.bounds().startDate, plan.activity.dayShockShape,
                       *engine_.rng, dayIndex);

  return actors::DayFrame{
      .day = day,
      .weekdayMult = math::counts::weekdayMultiplier(day.start),
      .seasonalMult =
          math::seasonal::dailyMultiplier(day.start, config_.seasonal),
  };
}

void Simulator::advanceLedgerToDay(const RunPlan &plan, RunState &state,
                                   const actors::DayFrame &frame) {
  const auto newBaseIdx = clearing::advanceBookThrough(
      engine_.ledger, plan.baseLedger.txns, state.baseIdx(),
      time::toEpochSeconds(frame.day.start),
      /*inclusive=*/false);

  state.setBaseIdx(newBaseIdx);
}

std::span<const std::uint32_t>
Simulator::updatePaydayState(const RunPlan &plan, RunState &state,
                             std::uint32_t dayIndex) {
  state.bumpAllDaysSincePayday();

  const auto paydayPersons = plan.payday.index.personsOn(dayIndex);
  for (const auto idx : paydayPersons) {
    state.resetDaysSincePayday(idx);
  }

  return paydayPersons;
}

void Simulator::advanceDynamics(const RunPlan &plan,
                                std::span<const std::uint32_t> paydayPersons) {
  cohort_.advanceAll(*engine_.rng, config_.dynamics, paydayPersons,
                     plan.population.sensitivities, dailyMultBuffer_);
}

void Simulator::draftSpenderTransactions(const RunPlan &plan, RunState &state,
                                         const actors::DayFrame &frame) {
  const routing::ResolvedAccounts resolved = plan.routing.resolvedAccounts();

  auto *lockArray = engine_.threadCount > 1 ? &lockArray_ : nullptr;

  const auto dailyMultipliers =
      std::span<const double>(dailyMultBuffer_.data(), dailyMultBuffer_.size());

  const SpenderEmissionPolicy emissionPolicy{
      .burst = config_.burst,
      .exploration = config_.exploration,
      .liquidity = config_.liquidity,
  };

  const std::size_t spenderCount = plan.population.spenders.size();

  if (engine_.threadCount <= 1) {
    SpenderEmissionLoop loop{
        market_,
        plan,
        state,
        frame,
        dailyMultipliers,
        emissionPolicy,
        resolved,
        ParallelLedgerView{engine_.ledger, lockArray},
    };

    loop.run(0, spenderCount, *engine_.rng, *engine_.factory, state.txns());

    return;
  }

  runParallel(engine_.threadCount, [&](std::uint32_t threadIdx) {
    const auto range =
        partitionRange(spenderCount, engine_.threadCount, threadIdx);

    if (range.size() == 0) {
      return;
    }

    auto &threadState = threadStates_[threadIdx];
    const auto threadFactory = engine_.factory->rebound(threadState.rng);

    SpenderEmissionLoop loop{
        market_,
        plan,
        state,
        frame,
        dailyMultipliers,
        emissionPolicy,
        resolved,
        ParallelLedgerView{engine_.ledger, lockArray},
    };

    loop.run(range.begin, range.end, threadState.rng, threadFactory,
             threadState.txns);
  });
}

} // namespace PhantomLedger::spending::simulator
