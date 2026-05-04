#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/explore.hpp"
#include "phantomledger/spending/clearing/parallel_ledger_view.hpp"
#include "phantomledger/spending/liquidity/multiplier.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/routing/emission_result.hpp"
#include "phantomledger/spending/simulator/prepared_run.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

class SpenderEmissionLoop {
public:
  struct Rules {
    double baseExploreP = 0.0;
    const actors::ExploreModifiers &exploration;
    const liquidity::Throttle &liquidity;
  };

  class RateSampler {
  public:
    RateSampler(const PreparedRun::Budget &budget, RunState &state,
                const actors::DayFrame &frame, Rules rules) noexcept;

    RateSampler &dailyMultipliers(std::span<const double> value) noexcept;
    RateSampler &ledgerView(ParallelLedgerView value) noexcept;

    [[nodiscard]] double combinedMultiplierFor(std::uint32_t personIndex) const;

    [[nodiscard]] double
    liquidityMultiplierFor(const spenders::PreparedSpender &prepared);

    [[nodiscard]] double latentBaseRateFor(const actors::Spender &spender,
                                           double combinedMult,
                                           double liquidityMult) const;

    [[nodiscard]] std::uint32_t
    transactionCountFor(random::Rng &rng, const actors::Spender &spender,
                        double latentBaseRate, double combinedMult,
                        double liquidityMult) const;

    [[nodiscard]] double exploreProbabilityFor(const actors::Spender &spender,
                                               double liquidityMult) const;

    [[nodiscard]] ::PhantomLedger::time::TimePoint
    timestampAtOffset(std::int32_t offsetSec) const noexcept;

    void consumeOnePersonDay() noexcept;
    void recordAccepted(std::uint32_t count) noexcept;

  private:
    [[nodiscard]] double
    availableCashFor(const spenders::PreparedSpender &prepared);

    const PreparedRun::Budget &budget_;
    RunState &state_;
    const actors::DayFrame &frame_;
    std::span<const double> dailyMultipliers_{};
    Rules rules_;
    ParallelLedgerView ledgerView_{};
  };

  class PaymentEmitter {
  public:
    PaymentEmitter(const market::Market &market,
                   const PreparedRun::Routing &routing,
                   const routing::ResolvedAccounts &resolved,
                   ParallelLedgerView ledgerView) noexcept;

    [[nodiscard]] std::optional<transactions::Transaction>
    tryEmit(random::Rng &rng, const actors::Event &event);

  private:
    [[nodiscard]] bool accept(const routing::EmissionResult &result);

    const market::Market &market_;
    const PreparedRun::Routing &routing_;
    const routing::ResolvedAccounts &resolved_;
    ParallelLedgerView ledgerView_{};
  };

  SpenderEmissionLoop(const PreparedRun::Population &population,
                      RateSampler &rates, PaymentEmitter &payments) noexcept;

  void run(std::size_t begin, std::size_t end, random::Rng &rng,
           const transactions::Factory &factory,
           std::vector<transactions::Transaction> &outTxns);

private:
  const PreparedRun::Population &population_;
  RateSampler &rates_;
  PaymentEmitter &payments_;
};

} // namespace PhantomLedger::spending::simulator
