#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/simulator/commerce_evolver.hpp"
#include "phantomledger/spending/simulator/day_source.hpp"
#include "phantomledger/spending/simulator/population_dynamics.hpp"
#include "phantomledger/spending/simulator/prepared_run.hpp"
#include "phantomledger/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::spending::simulator {

class DayDriver {
public:
  DayDriver() = default;

  DayDriver(DaySource days, CommerceEvolver commerce,
            PopulationDynamics dynamics, SpenderEmissionDriver emission);

  DayDriver(const DayDriver &) = delete;
  DayDriver &operator=(const DayDriver &) = delete;
  DayDriver(DayDriver &&) noexcept = default;
  DayDriver &operator=(DayDriver &&) noexcept = default;

  void prepare(market::Market &market, random::Rng &rng,
               const transactions::Factory &factory, clearing::Ledger *ledger,
               SpenderEmissionDriver::Threads emissionThreads,
               double txnsPerMonth);

  void bindEmission(const PreparedRun::Budget &budget,
                    const PreparedRun::Routing &routing) noexcept;

  void runDay(market::Market &market, random::Rng &rng,
              clearing::Ledger *ledger, const PreparedRun &run, RunState &state,
              std::uint32_t dayIndex);

  void finish(RunState &state);

  [[nodiscard]] std::span<const double> sensitivities() const noexcept;

private:
  void advanceLedgerToDay(clearing::Ledger *ledger,
                          const PreparedRun::LedgerReplay &replay,
                          RunState &state, const actors::DayFrame &frame) const;

  [[nodiscard]] std::span<const std::uint32_t>
  updatePaydayState(const PreparedRun::Paydays &paydays, RunState &state,
                    std::uint32_t dayIndex) const;

  DaySource days_{};
  CommerceEvolver commerce_{};
  PopulationDynamics dynamics_{};
  SpenderEmissionDriver emission_{};
};

} // namespace PhantomLedger::spending::simulator
