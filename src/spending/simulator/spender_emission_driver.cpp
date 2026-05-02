#include "phantomledger/spending/simulator/spender_emission_driver.hpp"

#include "phantomledger/spending/clearing/parallel_ledger_view.hpp"
#include "phantomledger/spending/simulator/loop.hpp"
#include "phantomledger/spending/simulator/thread_runner.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace PhantomLedger::spending::simulator {
namespace {

constexpr double kTxnReserveSlack = 1.05;

[[nodiscard]] std::string_view renderUInt(std::array<char, 16> &buf,
                                          std::uint32_t value) noexcept {
  auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
  (void)ec;
  return std::string_view(buf.data(),
                          static_cast<std::size_t>(ptr - buf.data()));
}

} // namespace

SpenderEmissionDriver::SpenderEmissionDriver(EmissionBehavior behavior)
    : behavior_(behavior) {}

void SpenderEmissionDriver::prepare(const market::Market &market,
                                    const Engine &engine,
                                    const TransactionLoad &load) {
  prepareThreadStates(market, engine, load);
  prepareLockArray(engine);
}

void SpenderEmissionDriver::prepareThreadStates(const market::Market &market,
                                                const Engine &engine,
                                                const TransactionLoad &load) {
  threadStates_.clear();
  if (engine.threadCount <= 1) {
    return;
  }

  if (engine.rngFactory == nullptr) {
    throw std::runtime_error(
        "spending::SpenderEmissionDriver: threadCount > 1 requires "
        "engine.rngFactory");
  }
  if (engine.factory == nullptr) {
    throw std::runtime_error(
        "spending::SpenderEmissionDriver: threadCount > 1 requires "
        "engine.factory (per-thread Factory is built via factory->rebound)");
  }

  threadStates_.reserve(engine.threadCount);

  const auto perThreadReserve = static_cast<std::size_t>(
      (static_cast<double>(market.bounds().days) *
       (static_cast<double>(market.population().count()) / 30.0) *
       load.txnsPerMonth * kTxnReserveSlack) /
      static_cast<double>(engine.threadCount));

  std::array<char, 16> idBuf{};
  for (std::uint32_t t = 0; t < engine.threadCount; ++t) {
    const auto idStr = renderUInt(idBuf, t);
    auto threadRng = engine.rngFactory->rng({"spending_thread", idStr});
    threadStates_.emplace_back(std::move(threadRng));
    threadStates_.back().txns.reserve(perThreadReserve);
  }
}

void SpenderEmissionDriver::prepareLockArray(const Engine &engine) {
  if (engine.threadCount <= 1 || engine.ledger == nullptr) {
    lockArray_.resize(0);
    return;
  }

  lockArray_.resize(static_cast<std::size_t>(engine.ledger->size()));
}

void SpenderEmissionDriver::mergeThreadTxns(RunState &state) {
  if (threadStates_.empty()) {
    return;
  }

  std::size_t total = state.txns().size();
  for (const auto &threadState : threadStates_) {
    total += threadState.txns.size();
  }

  state.txns().reserve(total);
  for (auto &threadState : threadStates_) {
    state.txns().insert(state.txns().end(),
                        std::make_move_iterator(threadState.txns.begin()),
                        std::make_move_iterator(threadState.txns.end()));
    threadState.txns.clear();
  }
}

void SpenderEmissionDriver::emitDay(const market::Market &market,
                                    const Engine &engine, const RunPlan &plan,
                                    RunState &state,
                                    const actors::DayFrame &frame,
                                    std::span<const double> dailyMultipliers) {
  const routing::ResolvedAccounts resolved = plan.routing.resolvedAccounts();
  auto *lockArray = engine.threadCount > 1 ? &lockArray_ : nullptr;

  const SpenderEmissionPolicy emissionPolicy{
      .baseExploreP = behavior_.baseExploreP,
      .burst = behavior_.burst,
      .exploration = behavior_.exploration,
      .liquidity = behavior_.liquidity,
  };

  const std::size_t spenderCount = plan.population.spenders.size();

  if (engine.threadCount <= 1) {
    SpenderEmissionLoop loop{market,
                             plan,
                             state,
                             frame,
                             dailyMultipliers,
                             emissionPolicy,
                             resolved,
                             ParallelLedgerView{engine.ledger, lockArray}};

    loop.run(0, spenderCount, *engine.rng, *engine.factory, state.txns());
    return;
  }

  runParallel(engine.threadCount, [&](std::uint32_t threadIdx) {
    const auto range =
        partitionRange(spenderCount, engine.threadCount, threadIdx);

    if (range.size() == 0) {
      return;
    }

    auto &threadState = threadStates_[threadIdx];
    const auto threadFactory = engine.factory->rebound(threadState.rng);

    SpenderEmissionLoop loop{market,
                             plan,
                             state,
                             frame,
                             dailyMultipliers,
                             emissionPolicy,
                             resolved,
                             ParallelLedgerView{engine.ledger, lockArray}};

    loop.run(range.begin, range.end, threadState.rng, threadFactory,
             threadState.txns);
  });
}

void SpenderEmissionDriver::finish(RunState &state) { mergeThreadTxns(state); }

} // namespace PhantomLedger::spending::simulator
