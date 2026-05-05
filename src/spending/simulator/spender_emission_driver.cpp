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

[[nodiscard]] SpenderEmissionLoop::Rules
rulesFrom(const SpenderEmissionDriver::Behavior &behavior) noexcept {
  return SpenderEmissionLoop::Rules{
      .baseExploreP = behavior.baseExploreP,
      .exploration = behavior.exploration,
      .liquidity = behavior.liquidity,
  };
}

} // namespace

SpenderEmissionDriver::SpenderEmissionDriver(Behavior behavior)
    : behavior_(behavior) {}

SpenderEmissionDriver &
SpenderEmissionDriver::bindMarket(const market::Market &value) noexcept {
  market_ = &value;
  return *this;
}

SpenderEmissionDriver &
SpenderEmissionDriver::bindRng(random::Rng &value) noexcept {
  rng_ = &value;
  return *this;
}

SpenderEmissionDriver &SpenderEmissionDriver::bindFactory(
    const transactions::Factory &value) noexcept {
  factory_ = &value;
  return *this;
}

SpenderEmissionDriver &
SpenderEmissionDriver::bindLedger(clearing::Ledger *value) noexcept {
  ledger_ = value;
  return *this;
}

SpenderEmissionDriver &SpenderEmissionDriver::threads(Threads value) noexcept {
  threads_ = value;
  return *this;
}

void SpenderEmissionDriver::prepare(double txnsPerMonth) {
  (void)market();
  (void)rng();
  (void)factory();

  budget_ = nullptr;
  routing_ = nullptr;

  prepareThreadStates(txnsPerMonth);
  prepareLockArray();
}

void SpenderEmissionDriver::bindRun(
    const PreparedRun::Budget &budget,
    const PreparedRun::Routing &routing) noexcept {
  budget_ = &budget;
  routing_ = &routing;
}

const market::Market &SpenderEmissionDriver::market() const {
  if (market_ == nullptr) {
    throw std::logic_error("spending::SpenderEmissionDriver: bindMarket must "
                           "be called before prepare or emission");
  }
  return *market_;
}

random::Rng &SpenderEmissionDriver::rng() const {
  if (rng_ == nullptr) {
    throw std::logic_error("spending::SpenderEmissionDriver: bindRng must "
                           "be called before prepare or emission");
  }
  return *rng_;
}

const transactions::Factory &SpenderEmissionDriver::factory() const {
  if (factory_ == nullptr) {
    throw std::logic_error("spending::SpenderEmissionDriver: bindFactory must "
                           "be called before prepare or emission");
  }
  return *factory_;
}

clearing::Ledger *SpenderEmissionDriver::ledger() const noexcept {
  return ledger_;
}

const SpenderEmissionDriver::Threads &
SpenderEmissionDriver::threading() const noexcept {
  return threads_;
}

const PreparedRun::Budget &SpenderEmissionDriver::budget() const {
  if (budget_ == nullptr) {
    throw std::logic_error("spending::SpenderEmissionDriver: bindRun must be "
                           "called before emission");
  }
  return *budget_;
}

const PreparedRun::Routing &SpenderEmissionDriver::routing() const {
  if (routing_ == nullptr) {
    throw std::logic_error("spending::SpenderEmissionDriver: bindRun must be "
                           "called before emission");
  }
  return *routing_;
}

void SpenderEmissionDriver::prepareThreadStates(double txnsPerMonth) {
  threadStates_.clear();

  const auto &threadCfg = threading();
  if (!threadCfg.parallel()) {
    return;
  }

  if (threadCfg.rngFactory == nullptr) {
    throw std::runtime_error(
        "spending::SpenderEmissionDriver: parallel emission requires "
        "Threads::rngFactory");
  }

  threadStates_.reserve(threadCfg.count);

  const auto perThreadReserve = static_cast<std::size_t>(
      (static_cast<double>(market().bounds().days) *
       (static_cast<double>(market().population().count()) / 30.0) *
       txnsPerMonth * kTxnReserveSlack) /
      static_cast<double>(threadCfg.count));

  std::array<char, 16> idBuf{};
  for (std::uint32_t t = 0; t < threadCfg.count; ++t) {
    const auto idStr = renderUInt(idBuf, t);
    auto threadRng = threadCfg.rngFactory->rng({"spending_thread", idStr});
    threadStates_.emplace_back(std::move(threadRng));
    threadStates_.back().txns.reserve(perThreadReserve);
  }
}

void SpenderEmissionDriver::prepareLockArray() {
  const auto &threadCfg = threading();
  if (!threadCfg.parallel() || ledger() == nullptr) {
    lockArray_.resize(0);
    return;
  }

  lockArray_.resize(static_cast<std::size_t>(ledger()->size()));
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

void SpenderEmissionDriver::emitSerial(
    const PreparedRun::Population &population, RunState &state,
    const actors::DayFrame &frame, std::span<const double> dailyMultipliers) {
  ParallelLedgerView ledgerView{ledger(), nullptr};
  SpenderEmissionLoop::RateSampler rates{budget(), state, frame,
                                         rulesFrom(behavior_)};
  rates.dailyMultipliers(dailyMultipliers).ledgerView(ledgerView);

  const auto &routingSnapshot = routing();
  SpenderEmissionLoop::PaymentEmitter payments{market(), routingSnapshot,
                                               factory(), ledgerView};
  SpenderEmissionLoop loop{population, rates, payments};

  loop.run(0, population.spenders.size(), rng(), state.txns());
}

void SpenderEmissionDriver::emitParallel(
    const PreparedRun::Population &population, RunState &state,
    const actors::DayFrame &frame, std::span<const double> dailyMultipliers) {
  const auto &threadCfg = threading();
  const auto spenderCount = population.spenders.size();
  const auto &routingSnapshot = routing();

  runParallel(threadCfg.count, [&](std::uint32_t threadIdx) {
    const auto range = partitionRange(spenderCount, threadCfg.count, threadIdx);

    if (range.size() == 0) {
      return;
    }

    auto &threadState = threadStates_[threadIdx];
    const auto threadFactory = factory().rebound(threadState.rng);

    ParallelLedgerView ledgerView{ledger(), &lockArray_};
    SpenderEmissionLoop::RateSampler rates{budget(), state, frame,
                                           rulesFrom(behavior_)};
    rates.dailyMultipliers(dailyMultipliers).ledgerView(ledgerView);

    SpenderEmissionLoop::PaymentEmitter payments{market(), routingSnapshot,
                                                 threadFactory, ledgerView};
    SpenderEmissionLoop loop{population, rates, payments};

    loop.run(range.begin, range.end, threadState.rng, threadState.txns);
  });
}

void SpenderEmissionDriver::emitDay(const PreparedRun::Population &population,
                                    RunState &state,
                                    const actors::DayFrame &frame,
                                    std::span<const double> dailyMultipliers) {
  if (!threading().parallel()) {
    emitSerial(population, state, frame, dailyMultipliers);
    return;
  }

  emitParallel(population, state, frame, dailyMultipliers);
}

void SpenderEmissionDriver::finish(RunState &state) { mergeThreadTxns(state); }

} // namespace PhantomLedger::spending::simulator
