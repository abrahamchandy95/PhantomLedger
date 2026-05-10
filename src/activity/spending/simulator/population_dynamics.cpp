#include "phantomledger/activity/spending/simulator/population_dynamics.hpp"

namespace PhantomLedger::spending::simulator {

PopulationDynamics::PopulationDynamics(dynamics::population::Drivers drivers)
    : drivers_(drivers) {}

void PopulationDynamics::resetFor(const market::Market &market) {
  const auto count = market.population().count();

  cohort_ = dynamics::population::Cohort(count);

  sensitivities_.resize(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto person = static_cast<entity::PersonId>(i + 1);
    sensitivities_[i] = market.population().object(person).payday.sensitivity;
  }

  dailyMultBuffer_.assign(count, 1.0);
}

std::span<const double> PopulationDynamics::sensitivities() const noexcept {
  return sensitivities_;
}

std::span<const double> PopulationDynamics::dailyMultipliers() const noexcept {
  return dailyMultBuffer_;
}

void PopulationDynamics::advance(random::Rng &rng,
                                 std::span<const std::uint32_t> paydayPersons) {
  cohort_.advanceAll(rng, drivers_,
                     dynamics::population::PaydaySignal{
                         .persons = paydayPersons,
                         .sensitivities = sensitivities_,
                     },
                     dailyMultBuffer_);
}

} // namespace PhantomLedger::spending::simulator
