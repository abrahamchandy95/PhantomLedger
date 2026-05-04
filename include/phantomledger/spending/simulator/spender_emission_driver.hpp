#pragma once

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/concurrent/account_lock_array.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/explore.hpp"
#include "phantomledger/spending/liquidity/multiplier.hpp"
#include "phantomledger/spending/market/market.hpp"
#include "phantomledger/spending/simulator/prepared_run.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::spending::simulator {

class SpenderEmissionDriver {
public:
  struct Behavior {
    double baseExploreP = 0.0;
    actors::ExploreModifiers exploration{};
    liquidity::Throttle liquidity{};
  };

  struct Threads {
    random::RngFactory *rngFactory = nullptr;
    std::uint32_t count = 1;

    [[nodiscard]] bool parallel() const noexcept { return count > 1; }
  };

  SpenderEmissionDriver() = default;
  explicit SpenderEmissionDriver(Behavior behavior);

  SpenderEmissionDriver(const SpenderEmissionDriver &) = delete;
  SpenderEmissionDriver &operator=(const SpenderEmissionDriver &) = delete;
  SpenderEmissionDriver(SpenderEmissionDriver &&) noexcept = default;
  SpenderEmissionDriver &operator=(SpenderEmissionDriver &&) noexcept = default;

  void prepare(const market::Market &market, random::Rng &rng,
               const transactions::Factory &factory, clearing::Ledger *ledger,
               Threads threads, double txnsPerMonth);

  void bindRun(const PreparedRun::Budget &budget,
               const PreparedRun::Routing &routing) noexcept;

  void emitDay(const PreparedRun::Population &population, RunState &state,
               const actors::DayFrame &frame,
               std::span<const double> dailyMultipliers);

  void finish(RunState &state);

private:
  [[nodiscard]] const market::Market &market() const;
  [[nodiscard]] random::Rng &rng() const;
  [[nodiscard]] const transactions::Factory &factory() const;
  [[nodiscard]] clearing::Ledger *ledger() const noexcept;
  [[nodiscard]] const Threads &threads() const noexcept;
  [[nodiscard]] const PreparedRun::Budget &budget() const;
  [[nodiscard]] const PreparedRun::Routing &routing() const;

  void prepareThreadStates(double txnsPerMonth);
  void prepareLockArray();
  void emitSerial(const PreparedRun::Population &population, RunState &state,
                  const actors::DayFrame &frame,
                  std::span<const double> dailyMultipliers);
  void emitParallel(const PreparedRun::Population &population, RunState &state,
                    const actors::DayFrame &frame,
                    std::span<const double> dailyMultipliers);
  void mergeThreadTxns(RunState &state);

  Behavior behavior_{};
  const market::Market *market_ = nullptr;
  random::Rng *rng_ = nullptr;
  const transactions::Factory *factory_ = nullptr;
  clearing::Ledger *ledger_ = nullptr;
  Threads threads_{};
  const PreparedRun::Budget *budget_ = nullptr;
  const PreparedRun::Routing *routing_ = nullptr;
  std::vector<ThreadLocalState> threadStates_{};
  primitives::concurrent::AccountLockArray lockArray_{};
};

} // namespace PhantomLedger::spending::simulator
