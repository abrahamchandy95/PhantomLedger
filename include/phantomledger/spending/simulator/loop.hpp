#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/clearing/parallel_ledger_view.hpp"
#include "phantomledger/spending/config/burst.hpp"
#include "phantomledger/spending/config/exploration.hpp"
#include "phantomledger/spending/config/liquidity.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/routing/emission_result.hpp"
#include "phantomledger/spending/simulator/plan.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

struct SpenderEmissionPolicy {
  const config::BurstBehavior &burst;
  const config::ExplorationHabits &exploration;
  const config::LiquidityConstraints &liquidity;
};

class SpenderEmissionLoop {
public:
  SpenderEmissionLoop(const market::Market &market, const RunPlan &plan,
                      RunState &state, const actors::DayFrame &frame,
                      std::span<const double> dailyMultipliers,
                      SpenderEmissionPolicy policy,
                      const routing::ResolvedAccounts &resolved,
                      ParallelLedgerView ledgerView) noexcept;

  void run(std::size_t begin, std::size_t end, random::Rng &rng,
           const transactions::Factory &factory,
           std::vector<transactions::Transaction> &outTxns);

private:
  [[nodiscard]] double
  availableCashFor(const spenders::PreparedSpender &prepared);

  [[nodiscard]] double
  liquidityMultiplierFor(const spenders::PreparedSpender &prepared);

  [[nodiscard]] double combinedMultiplierFor(std::uint32_t personIndex) const;

  [[nodiscard]] double latentBaseRateFor(const actors::Spender &spender,
                                         double combinedMult,
                                         double liquidityMult) const;

  [[nodiscard]] std::uint32_t
  transactionCountFor(random::Rng &rng, const actors::Spender &spender,
                      double latentBaseRate, double combinedMult,
                      double liquidityMult) const;

  [[nodiscard]] double exploreProbabilityFor(const actors::Spender &spender,
                                             double liquidityMult) const;

  [[nodiscard]] bool tryEmit(const routing::EmissionResult &result);

  const market::Market &market_;
  const RunPlan &plan_;
  RunState &state_;
  const actors::DayFrame &frame_;
  std::span<const double> dailyMultipliers_;
  SpenderEmissionPolicy policy_;
  const routing::ResolvedAccounts &resolved_;
  ParallelLedgerView ledgerView_;
};

} // namespace PhantomLedger::spending::simulator
