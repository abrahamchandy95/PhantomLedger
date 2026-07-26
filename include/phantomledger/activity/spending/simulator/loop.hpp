#pragma once

#include "phantomledger/activity/spending/actors/day.hpp"
#include "phantomledger/activity/spending/actors/event.hpp"
#include "phantomledger/activity/spending/actors/explore.hpp"
#include "phantomledger/activity/spending/liquidity/multiplier.hpp"
#include "phantomledger/activity/spending/market/market.hpp"
#include "phantomledger/activity/spending/routing/emission_result.hpp"
#include "phantomledger/activity/spending/simulator/gate.hpp"
#include "phantomledger/activity/spending/simulator/prepared_run.hpp"
#include "phantomledger/activity/spending/simulator/state.hpp"
#include "phantomledger/math/counts.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace PhantomLedger::activity::spending::simulator {

class SpenderEmissionLoop {
public:
  struct Rules {
    double baseExploreP = 0.0;
    const actors::ExploreModifiers &exploration;
    const liquidity::Throttle &liquidity;
    const math::counts::Rates &rates;
  };

  class RateSampler {
  public:
    RateSampler(const PreparedRun::Budget &budget, RunState &state,
                const actors::DayFrame &frame, Rules rules) noexcept;

    RateSampler &dailyMultipliers(std::span<const double> value) noexcept;
    RateSampler &ledgerView(Gate value) noexcept;

    [[nodiscard]] double combinedMultiplierFor(std::uint32_t personIndex) const;

    [[nodiscard]] double
    liquidityMultiplierFor(const spenders::PreparedSpender &prepared);

    struct DailyMultipliers {
      double combined = 1.0;
      double liquidity = 1.0;
    };

    [[nodiscard]] double latentBaseRateFor(const actors::Spender &spender,
                                           DailyMultipliers mults) const;

    [[nodiscard]] std::uint32_t
    transactionCountFor(random::Rng &rng, const actors::Spender &spender,
                        double latentBaseRate, DailyMultipliers mults) const;

    [[nodiscard]] double exploreProbabilityFor(const actors::Spender &spender,
                                               double liquidityMult) const;

    [[nodiscard]] ::PhantomLedger::time::TimePoint
    timestampAtOffset(std::int32_t offsetSec) const noexcept;

    void consumeOnePersonDay() noexcept;
    void recordAccepted(std::uint32_t count) noexcept;

    [[nodiscard]] double
    availableCashFor(const spenders::PreparedSpender &prepared);

    [[nodiscard]] double
    cardLiquidityFor(const spenders::PreparedSpender &prepared);

    [[nodiscard]] double lastLiquidityMult() const noexcept;
    [[nodiscard]] double lastAvailableToSpend() const noexcept;

    // H1 step 2b (class P): the day frame's CPI level multiplier,
    // computed once at construction — pure derived data, no draws.
    // Carried onto every Event (ticket scaling) and Snapshot
    // (screen scaling).
    [[nodiscard]] double dayPriceScale() const noexcept;

    // H2 step 2c: the frame's day index, so the loop can compare it to
    // each spender's retirement day (pure derived data, no draws).
    [[nodiscard]] std::uint32_t frameDayIndex() const noexcept;

  private:
    [[nodiscard]] double
    availableToSpendFor(const spenders::PreparedSpender &prepared);

    const PreparedRun::Budget &budget_;
    RunState &state_;
    const actors::DayFrame &frame_;
    std::span<const double> dailyMultipliers_{};
    Rules rules_;
    Gate ledgerView_{};

    double dayPriceScale_ = 1.0;
    // H4 (authority U-9): the day frame's REAL per-capita consumption
    // level (econ::realPceLevel), computed once at construction like
    // dayPriceScale_. Multiplied into every spender's combined rate
    // multiplier, so the session's COUNT axis follows the measured BEA
    // path while ticket AMOUNTS stay priceScale-realized — the
    // count-only channel law. The budget stays a CALIBRATION-LEVEL
    // target: a 2019 frame multiplies by exactly 1.0.
    double dayRealLevel_ = 1.0;
    double lastLiquidityMult_ = 0.0;
    double lastAvailableToSpend_ = 0.0;
  };

  class PaymentEmitter {
  public:
    // An accepted proposal: the transaction record for the stream,
    // and the indexed posting the day driver applies to the soft
    // ledger at the day boundary.
    struct Emitted {
      transactions::Transaction txn;
      clearing::Ledger::Posting posting;
    };

    PaymentEmitter(const market::Market &market,
                   const PreparedRun::Routing &routing,
                   const transactions::Factory &factory,
                   Gate ledgerView) noexcept;

    void bindRateSampler(const RateSampler *sampler) noexcept;

    [[nodiscard]] std::optional<Emitted> tryEmit(random::Rng &rng,
                                                 const actors::Event &event);

  private:
    const market::Market &market_;
    const PreparedRun::Routing &routing_;
    const transactions::Factory &factory_;
    routing::ResolvedAccounts resolved_{};
    Gate ledgerView_{};
    const RateSampler *rateSampler_ = nullptr;
  };

  SpenderEmissionLoop(const PreparedRun::Population &population,
                      RateSampler &rates, PaymentEmitter &payments) noexcept;

  void run(std::size_t begin, std::size_t end,
           std::span<random::Rng> spenderRngs,
           std::vector<transactions::Transaction> &outTxns,
           std::vector<clearing::Ledger::Posting> &outPostings);

private:
  const PreparedRun::Population &population_;
  RateSampler &rates_;
  PaymentEmitter &payments_;
};

} // namespace PhantomLedger::activity::spending::simulator
