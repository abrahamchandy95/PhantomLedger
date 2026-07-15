#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"

#include "phantomledger/activity/spending/simulator/gate.hpp"
#include "phantomledger/activity/spending/simulator/loop.hpp"
#include "phantomledger/activity/spending/simulator/thread_runner.hpp"
#include "phantomledger/primitives/random/factory.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace PhantomLedger::activity::spending::simulator {
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
      .rates = behavior.rates,
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
  // Trip the bound-state validators; throws if any required binding is missing.
  (void)market();
  (void)rng();
  (void)factory();

  budget_ = nullptr;
  routing_ = nullptr;

  prepareThreadStates(txnsPerMonth);
  preparePool();
  prepareSpenderRngs();
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

  const auto threadCount = std::max(threading().count, std::uint32_t{1});
  threadStates_.reserve(threadCount);

  // Per-thread scratch holds ONE day of one thread's emissions before the
  // daily merge drains it (clear() keeps capacity), so reserve for a heavy
  // day, not the whole run. The previous estimate reserved
  // people * months * txnsPerMonth / threadCount, the entire run's target,
  // in EVERY thread: at population 2,000 over 29 years that is ~245 MB per
  // thread, ~2.9 GB of permanently dead capacity across 12 threads.
  // Reservation is allocation-only; emitted values are unchanged.
  const auto people = static_cast<double>(market().population().count());
  const auto expectedPerDay = people * txnsPerMonth / 30.0 * kTxnReserveSlack;
  constexpr double kHeavyDayHeadroom = 8.0; // payday spikes run several x mean
  const auto perThreadReserve = static_cast<std::size_t>(
      std::max(64.0, kHeavyDayHeadroom * expectedPerDay /
                         static_cast<double>(threadCount)));

  for (std::uint32_t t = 0; t < threadCount; ++t) {
    threadStates_.emplace_back();
    threadStates_.back().txns.reserve(perThreadReserve);
  }
}

void SpenderEmissionDriver::prepareSpenderRngs() {
  if (threading().rngFactory == nullptr) {
    throw std::runtime_error(
        "spending::SpenderEmissionDriver: emission requires "
        "Threads::rngFactory (per-spender streams)");
  }

  const auto count = market().population().count();
  spenderRngs_.clear();
  spenderRngs_.reserve(count);

  std::array<char, 16> idBuf{};
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto idStr = renderUInt(idBuf, i);
    spenderRngs_.push_back(
        threading().rngFactory->rng({"spender_stream", idStr}));
  }
}

void SpenderEmissionDriver::mergeThreadTxns(RunState &state) {
  if (threadStates_.empty()) {
    return;
  }

  auto &dst = state.txns();

  std::size_t total = dst.size();
  std::size_t totalPostings = dayPostings_.size();
  for (const auto &threadState : threadStates_) {
    total += threadState.txns.size();
    totalPostings += threadState.postings.size();
  }
  // Account for rows already merged on previous days; reserve(total) alone
  // is a no-op once size() exceeds the day count and the insert below then
  // grows through doubling. Redundant when the driver pre-reserves the whole
  // run, kept as a correct local invariant.
  dst.reserve(dst.size() + total);
  dayPostings_.reserve(totalPostings);

  for (auto &threadState : threadStates_) {
    dst.insert(dst.end(), std::make_move_iterator(threadState.txns.begin()),
               std::make_move_iterator(threadState.txns.end()));
    threadState.txns.clear();

    dayPostings_.insert(dayPostings_.end(), threadState.postings.begin(),
                        threadState.postings.end());
    threadState.postings.clear();
  }
}

void SpenderEmissionDriver::preparePool() {
  const auto threadCount = std::max(threading().count, std::uint32_t{1});
  if (pool_ == nullptr || pool_->threadCount() != threadCount) {
    pool_ = std::make_unique<WorkerPool>(threadCount);
  }
}

void SpenderEmissionDriver::applyDayPostings() {
  auto *book = ledger();
  if (book != nullptr) {

    for (const auto &posting : dayPostings_) {
      (void)book->transfer(posting.srcIdx, posting.dstIdx, posting.amount,
                           posting.channel);
    }
  }
  dayPostings_.clear();
}

void SpenderEmissionDriver::emitDay(const PreparedRun::Population &population,
                                    RunState &state,
                                    const actors::DayFrame &frame,
                                    std::span<const double> dailyMultipliers) {
  const auto threadCount = std::max(threading().count, std::uint32_t{1});
  const auto spenderCount = population.spenders.size();
  const auto &routingSnapshot = routing();
  const Gate gate{ledger()};

  if (pool_ == nullptr) {
    preparePool();
  }
  const std::function<void(std::uint32_t)> emitBody =
      [&](std::uint32_t threadIdx) {
        const auto range = partitionRange(spenderCount, threadCount, threadIdx);
        if (range.size() == 0) {
          return;
        }

        auto &threadState = threadStates_[threadIdx];

        SpenderEmissionLoop::RateSampler rates{budget(), state, frame,
                                               rulesFrom(behavior_)};
        rates.dailyMultipliers(dailyMultipliers).ledgerView(gate);

        SpenderEmissionLoop::PaymentEmitter payments{market(), routingSnapshot,
                                                     factory(), gate};
        SpenderEmissionLoop loop{population, rates, payments};

        loop.run(range.begin, range.end, std::span<random::Rng>{spenderRngs_},
                 threadState.txns, threadState.postings);
      };
  pool_->run(emitBody);

  mergeThreadTxns(state);
  applyDayPostings();
}

void SpenderEmissionDriver::finish(RunState &state) { mergeThreadTxns(state); }

} // namespace PhantomLedger::activity::spending::simulator
