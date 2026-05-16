#pragma once

#include "phantomledger/activity/spending/actors/day.hpp"
#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/simulator/commerce_evolver.hpp"
#include "phantomledger/activity/spending/simulator/day_source.hpp"
#include "phantomledger/activity/spending/simulator/population_dynamics.hpp"
#include "phantomledger/activity/spending/simulator/prepared_run.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/activity/spending/simulator/state.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

class DayDriver {
public:
  DayDriver() = default;

  DayDriver(DaySource days, CommerceEvolver commerce,
            PopulationDynamics dynamics, SpenderEmissionDriver emission);

  DayDriver(const DayDriver &) = delete;
  DayDriver &operator=(const DayDriver &) = delete;
  DayDriver(DayDriver &&) noexcept = default;
  DayDriver &operator=(DayDriver &&) noexcept = default;

  void resetFor(const market::Market &market);

  void bindMarket(market::Market &value) noexcept;
  void bindRng(random::Rng &value) noexcept;
  void bindLedger(clearing::Ledger *value) noexcept;
  void bindFactory(const transactions::Factory &value) noexcept;

  void emissionThreads(SpenderEmissionDriver::Threads threads) noexcept;
  void prepareEmission(double txnsPerMonth);

  void bindEmission(const PreparedRun::Budget &budget,
                    const PreparedRun::Routing &routing) noexcept;

  void
  bindCardCycleDriver(::PhantomLedger::transfers::credit_cards::CardCycleDriver
                          *cards) noexcept;

  void runDay(const PreparedRun &run, RunState &state, std::uint32_t dayIndex);

  void finish(RunState &state);

  [[nodiscard]] std::vector<transactions::Transaction> drainCardCycles();

  [[nodiscard]] std::span<const double> sensitivities() const noexcept;

private:
  void rebindEmitter() noexcept;

  [[nodiscard]] market::Market &boundMarket() const;
  [[nodiscard]] random::Rng &boundRng() const;

  void advanceLedgerToDay(const PreparedRun::LedgerReplay &replay,
                          RunState &state, const actors::DayFrame &frame) const;

  [[nodiscard]] std::span<const std::uint32_t>
  updatePaydayState(const PreparedRun::Paydays &paydays, RunState &state,
                    std::uint32_t dayIndex) const;

  void ingestEmittedTo(
      ::PhantomLedger::transfers::credit_cards::CardCycleDriver *cards,
      const RunState &state);

  DaySource days_{};
  CommerceEvolver commerce_{};
  PopulationDynamics dynamics_{};
  SpenderEmissionDriver emission_{};

  market::Market *market_ = nullptr;
  random::Rng *rng_ = nullptr;
  const transactions::Factory *factory_ = nullptr;
  clearing::Ledger *ledger_ = nullptr;
  ::PhantomLedger::transfers::credit_cards::CardCycleDriver *cards_ = nullptr;
  std::size_t cardIngestCursor_ = 0;
};

} // namespace PhantomLedger::activity::spending::simulator
