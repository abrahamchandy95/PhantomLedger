#pragma once

#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/obligations/snapshot.hpp"
#include "phantomledger/activity/spending/simulator/day_driver.hpp"
#include "phantomledger/activity/spending/simulator/run_planner.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/card_cycle_driver.hpp"

#include <vector>

namespace PhantomLedger::activity::spending::simulator {

class Simulator {
public:
  Simulator(market::Market &market, random::Rng &rng,
            const transactions::Factory &factory,
            const obligations::Snapshot &obligations);

  Simulator(const Simulator &) = delete;
  Simulator &operator=(const Simulator &) = delete;
  Simulator(Simulator &&) noexcept = default;
  Simulator &operator=(Simulator &&) = delete;

  Simulator &ledger(clearing::Ledger *ledger) noexcept;
  Simulator &planner(RunPlanner planner) noexcept;
  Simulator &dayDriver(DayDriver dayDriver) noexcept;
  Simulator &emissionThreads(SpenderEmissionDriver::Threads threads) noexcept;
  Simulator &
  cardCycleDriver(::PhantomLedger::transfers::credit_cards::CardCycleDriver
                      *cards) noexcept;

  [[nodiscard]] std::vector<transactions::Transaction> run();

private:
  market::Market &market_;
  random::Rng &rng_;
  const transactions::Factory &factory_;
  const obligations::Snapshot &obligations_;
  clearing::Ledger *ledger_ = nullptr;
  SpenderEmissionDriver::Threads emissionThreads_{};
  ::PhantomLedger::transfers::credit_cards::CardCycleDriver *cardCycleDriver_ =
      nullptr;

  RunPlanner planner_{};
  DayDriver dayDriver_{};
};

} // namespace PhantomLedger::activity::spending::simulator
