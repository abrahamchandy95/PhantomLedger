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

void DayDriver::prepare(market::Market &market, random::Rng &rng,
                        const transactions::Factory &factory,
                        clearing::Ledger *ledger,
                        SpenderEmissionDriver::Threads emissionThreads,
                        double txnsPerMonth) {
  dynamics_.resetFor(market);
  emission_.prepare(market, rng, factory, ledger, emissionThreads,
                    txnsPerMonth);
}

void DayDriver::bindEmission(const PreparedRun::Budget &budget,
                             const PreparedRun::Routing &routing) noexcept {
  emission_.bindRun(budget, routing);
}

std::span<const double> DayDriver::sensitivities() const noexcept {
  return dynamics_.sensitivities();
}

void DayDriver::runDay(market::Market &market, random::Rng &rng,
                       clearing::Ledger *ledger, const PreparedRun &run,
                       RunState &state, std::uint32_t dayIndex) {
  commerce_.evolveIfNeeded(market, rng, dayIndex);

  const auto frame = days_.build(market.bounds(), rng, dayIndex);

  advanceLedgerToDay(ledger, run.ledgerReplay(), state, frame);

  const auto paydayPersons = updatePaydayState(run.paydays(), state, dayIndex);

  dynamics_.advance(rng, paydayPersons);

  emission_.emitDay(run.population(), state, frame,
                    dynamics_.dailyMultipliers());
}

void DayDriver::advanceLedgerToDay(clearing::Ledger *ledger,
                                   const PreparedRun::LedgerReplay &replay,
                                   RunState &state,
                                   const actors::DayFrame &frame) const {
  const auto newBaseIdx =
      clearing::advanceBookThrough(ledger, replay.txns, state.baseIdx(),
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
