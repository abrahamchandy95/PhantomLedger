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

SpenderEmissionDriver::SpenderEmissionDriver(Behavior behavior)
    : behavior_(behavior) {}

void SpenderEmissionDriver::prepare(const market::Market &market,
                                    const RunResources &resources,
                                    double txnsPerMonth) {
  prepareThreadStates(market, resources, txnsPerMonth);
  prepareLockArray(resources);
}

void SpenderEmissionDriver::prepareThreadStates(const market::Market &market,
                                                const RunResources &resources,
                                                double txnsPerMonth) {
  threadStates_.clear();

  const auto &threads = resources.threads();
  if (!threads.parallel()) {
    return;
  }

  if (threads.rngFactory == nullptr) {
    throw std::runtime_error(
        "spending::SpenderEmissionDriver: parallel emission requires "
        "resources.threads().rngFactory");
  }

  threadStates_.reserve(threads.count);

  const auto perThreadReserve = static_cast<std::size_t>(
      (static_cast<double>(market.bounds().days) *
       (static_cast<double>(market.population().count()) / 30.0) *
       txnsPerMonth * kTxnReserveSlack) /
      static_cast<double>(threads.count));

  std::array<char, 16> idBuf{};
  for (std::uint32_t t = 0; t < threads.count; ++t) {
    const auto idStr = renderUInt(idBuf, t);
    auto threadRng = threads.rngFactory->rng({"spending_thread", idStr});
    threadStates_.emplace_back(std::move(threadRng));
    threadStates_.back().txns.reserve(perThreadReserve);
  }
}

void SpenderEmissionDriver::prepareLockArray(const RunResources &resources) {
  const auto &threads = resources.threads();
  if (!threads.parallel() || resources.ledger() == nullptr) {
    lockArray_.resize(0);
    return;
  }

  lockArray_.resize(static_cast<std::size_t>(resources.ledger()->size()));
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
                                    const RunResources &resources,
                                    const PreparedRun::Population &population,
                                    const PreparedRun::Budget &budget,
                                    const PreparedRun::Routing &routingSnapshot,
                                    RunState &state,
                                    const actors::DayFrame &frame,
                                    std::span<const double> dailyMultipliers) {
  const routing::ResolvedAccounts resolved = routingSnapshot.resolvedAccounts();
  const auto &threads = resources.threads();
  auto *lockArray = threads.parallel() ? &lockArray_ : nullptr;

  const SpenderEmissionLoop::Rules rules{
      .baseExploreP = behavior_.baseExploreP,
      .exploration = behavior_.exploration,
      .liquidity = behavior_.liquidity,
  };

  const std::size_t spenderCount = population.spenders.size();

  if (!threads.parallel()) {
    ParallelLedgerView ledgerView{resources.ledger(), lockArray};
    SpenderEmissionLoop::RateSampler rates{budget, state, frame, rules};
    rates.dailyMultipliers(dailyMultipliers).ledgerView(ledgerView);

    SpenderEmissionLoop::PaymentEmitter payments{market, routingSnapshot,
                                                 resolved, ledgerView};
    SpenderEmissionLoop loop{population, rates, payments};

    loop.run(0, spenderCount, resources.rng(), resources.factory(),
             state.txns());
    return;
  }

  runParallel(threads.count, [&](std::uint32_t threadIdx) {
    const auto range = partitionRange(spenderCount, threads.count, threadIdx);

    if (range.size() == 0) {
      return;
    }

    auto &threadState = threadStates_[threadIdx];
    const auto threadFactory = resources.factory().rebound(threadState.rng);

    ParallelLedgerView ledgerView{resources.ledger(), lockArray};
    SpenderEmissionLoop::RateSampler rates{budget, state, frame, rules};
    rates.dailyMultipliers(dailyMultipliers).ledgerView(ledgerView);

    SpenderEmissionLoop::PaymentEmitter payments{market, routingSnapshot,
                                                 resolved, ledgerView};
    SpenderEmissionLoop loop{population, rates, payments};

    loop.run(range.begin, range.end, threadState.rng, threadFactory,
             threadState.txns);
  });
}

void SpenderEmissionDriver::finish(RunState &state) { mergeThreadTxns(state); }

} // namespace PhantomLedger::spending::simulator
