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

void DayDriver::prepare(market::Market &market, const RunResources &resources,
                        double txnsPerMonth) {
  dynamics_.resetFor(market);
  emission_.prepare(market, resources, txnsPerMonth);
}

std::span<const double> DayDriver::sensitivities() const noexcept {
  return dynamics_.sensitivities();
}

void DayDriver::runDay(market::Market &market, const RunResources &resources,
                       const PreparedRun &run, RunState &state,
                       std::uint32_t dayIndex) {
  commerce_.evolveIfNeeded(market, resources.rng(), dayIndex);

  const auto frame = days_.build(market.bounds(), resources.rng(), dayIndex);

  advanceLedgerToDay(resources, run.ledgerReplay(), state, frame);

  const auto paydayPersons = updatePaydayState(run.paydays(), state, dayIndex);

  dynamics_.advance(resources.rng(), paydayPersons);

  emission_.emitDay(market, resources, run.population(), run.budget(),
                    run.routing(), state, frame, dynamics_.dailyMultipliers());
}

void DayDriver::advanceLedgerToDay(const RunResources &resources,
                                   const PreparedRun::LedgerReplay &replay,
                                   RunState &state,
                                   const actors::DayFrame &frame) const {
  const auto newBaseIdx = clearing::advanceBookThrough(
      resources.ledger(), replay.txns, state.baseIdx(),
      time::toEpochSeconds(frame.day.start),
      /*inclusive=*/false);

  state.setBaseIdx(newBaseIdx);
}

std::span<const std::uint32_t>
DayDriver::updatePaydayState(const PreparedRun::Paydays &paydays,
                             RunState &state, std::uint32_t dayIndex) const {
  state.bumpAllDaysSincePayday();

  const auto paydayPersons = paydays.index.personsOn(dayIndex);
  for (const auto idx : paydayPersons) {
    state.resetDaysSincePayday(idx);
  }

  return paydayPersons;
}

void DayDriver::finish(RunState &state) { emission_.finish(state); }

} // namespace PhantomLedger::spending::simulator
