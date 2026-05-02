#include "phantomledger/spending/simulator/day_driver.hpp"

#include "phantomledger/transactions/clearing/screening.hpp"

#include <cstdint>
#include <span>
#include <utility>

namespace PhantomLedger::spending::simulator {

DayDriver::DayDriver(DaySource days, CommerceEvolver commerce,
                     PopulationDynamics dynamics,
                     SpenderEmissionDriver emission)
    : days_(std::move(days)), commerce_(std::move(commerce)),
      dynamics_(std::move(dynamics)), emission_(std::move(emission)) {}

void DayDriver::prepare(market::Market &market, const Engine &engine,
                        const TransactionLoad &load) {
  dynamics_.resetFor(market);
  emission_.prepare(market, engine, load);
}

std::span<const double> DayDriver::sensitivities() const noexcept {
  return dynamics_.sensitivities();
}

void DayDriver::runDay(market::Market &market, const Engine &engine,
                       const RunPlan &plan, RunState &state,
                       std::uint32_t dayIndex) {
  commerce_.evolveIfNeeded(market, *engine.rng, dayIndex);

  const auto frame = days_.build(market.bounds(), *engine.rng, dayIndex);

  advanceLedgerToDay(engine, plan, state, frame);

  const auto paydayPersons = updatePaydayState(plan, state, dayIndex);

  dynamics_.advance(*engine.rng, paydayPersons);

  emission_.emitDay(market, engine, plan, state, frame,
                    dynamics_.dailyMultipliers());
}

void DayDriver::advanceLedgerToDay(const Engine &engine, const RunPlan &plan,
                                   RunState &state,
                                   const actors::DayFrame &frame) const {
  const auto newBaseIdx = clearing::advanceBookThrough(
      engine.ledger, plan.baseLedger.txns, state.baseIdx(),
      time::toEpochSeconds(frame.day.start),
      /*inclusive=*/false);

  state.setBaseIdx(newBaseIdx);
}

std::span<const std::uint32_t>
DayDriver::updatePaydayState(const RunPlan &plan, RunState &state,
                             std::uint32_t dayIndex) const {
  state.bumpAllDaysSincePayday();

  const auto paydayPersons = plan.payday.index.personsOn(dayIndex);
  for (const auto idx : paydayPersons) {
    state.resetDaysSincePayday(idx);
  }

  return paydayPersons;
}

void DayDriver::finish(RunState &state) { emission_.finish(state); }

} // namespace PhantomLedger::spending::simulator
