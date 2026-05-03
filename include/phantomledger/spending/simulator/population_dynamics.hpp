#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/dynamics/population/advance.hpp"
#include "phantomledger/spending/market/market.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

class PopulationDynamics {
public:
  PopulationDynamics() = default;
  explicit PopulationDynamics(dynamics::population::Drivers drivers);

  void resetFor(const market::Market &market);

  [[nodiscard]] std::span<const double> sensitivities() const noexcept;
  [[nodiscard]] std::span<const double> dailyMultipliers() const noexcept;

  void advance(random::Rng &rng, std::span<const std::uint32_t> paydayPersons);

private:
  dynamics::population::Drivers drivers_{};
  dynamics::population::Cohort cohort_{};
  std::vector<double> sensitivities_{};
  std::vector<double> dailyMultBuffer_{};
};

} // namespace PhantomLedger::spending::simulator
