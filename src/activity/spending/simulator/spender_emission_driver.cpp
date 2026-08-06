#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"

#include "phantomledger/activity/spending/simulator/gate.hpp"
#include "phantomledger/activity/spending/simulator/loop.hpp"
#include "phantomledger/activity/spending/simulator/thread_runner.hpp"
#include "phantomledger/primitives/random/factory.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace PhantomLedger::activity::spending::simulator {

namespace detail {

void appendAcceptedDayPostings(
    clearing::Ledger *book, std::vector<transactions::Transaction> &destination,
    std::vector<transactions::Transaction> &dayTransactions,
    std::vector<clearing::Ledger::Posting> &dayPostings) {
  if (dayTransactions.size() != dayPostings.size()) {
    throw std::logic_error(
        "spending day transaction/posting cardinality mismatch");
  }
  for (std::size_t i = 0; i < dayTransactions.size(); ++i) {
    if (dayTransactions[i].timestamp != dayPostings[i].timestamp) {
      throw std::logic_error("spending transaction/posting timestamp mismatch");
    }
  }

  destination.reserve(destination.size() + dayTransactions.size());

  std::vector<std::size_t> replayOrder(dayTransactions.size());
  std::iota(replayOrder.begin(), replayOrder.end(), std::size_t{0});
  const transactions::Comparator less{
      transactions::Comparator::Scope::fundsTransfer};
  std::stable_sort(replayOrder.begin(), replayOrder.end(),
                   [&](std::size_t lhs, std::size_t rhs) {
                     return less(dayTransactions[lhs], dayTransactions[rhs]);
                   });

  for (const auto i : replayOrder) {
    bool accepted = true;
    if (book != nullptr) {
      accepted = book->transferAt(dayPostings[i]).accepted();
    }

    if (accepted) {
      destination.push_back(std::move(dayTransactions[i]));
    }
  }

  dayTransactions.clear();
  dayPostings.clear();
}

} // namespace detail

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

  prepareThreadScratch(txnsPerMonth);
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

void SpenderEmissionDriver::prepareThreadScratch(double txnsPerMonth) {
  threadScratch_.clear();

  const auto threadCount = std::max(threading().count, std::uint32_t{1});
  threadScratch_.reserve(threadCount);

  const auto people = static_cast<double>(market().population().count());
  const auto expectedPerDay = people * txnsPerMonth / 30.0 * kTxnReserveSlack;
  constexpr double kHeavyDayHeadroom = 8.0; // payday spikes run several x mean
  const auto perThreadReserve = static_cast<std::size_t>(
      std::max(64.0, kHeavyDayHeadroom * expectedPerDay /
                         static_cast<double>(threadCount)));

  for (std::uint32_t t = 0; t < threadCount; ++t) {
    threadScratch_.emplace_back();
    threadScratch_.back().txns.reserve(perThreadReserve);
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
  if (threadScratch_.empty()) {
    return;
  }

  auto &dst = state.txns();

  std::size_t total = 0;
  for (const auto &scratch : threadScratch_) {
    if (scratch.txns.size() != scratch.postings.size()) {
      throw std::logic_error(
          "spending thread transaction/posting cardinality mismatch");
    }
    total += scratch.txns.size();
  }

  dayTransactions_.reserve(dayTransactions_.size() + total);
  dayPostings_.reserve(dayPostings_.size() + total);

  for (auto &scratch : threadScratch_) {
    dayTransactions_.insert(dayTransactions_.end(),
                            std::make_move_iterator(scratch.txns.begin()),
                            std::make_move_iterator(scratch.txns.end()));
    dayPostings_.insert(dayPostings_.end(), scratch.postings.begin(),
                        scratch.postings.end());
    scratch.txns.clear();
    scratch.postings.clear();
  }

  // Settle the full cross-worker day as one batch. Sorting inside the helper
  // matches canonical replay order and avoids thread partition order deciding
  // which of two competing same-day transfers survives.
  detail::appendAcceptedDayPostings(ledger(), dst, dayTransactions_,
                                    dayPostings_);
}

void SpenderEmissionDriver::preparePool() {
  const auto threadCount = std::max(threading().count, std::uint32_t{1});
  if (pool_ == nullptr || pool_->threadCount() != threadCount) {
    pool_ = std::make_unique<WorkerPool>(threadCount);
  }
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

        auto &scratch = threadScratch_[threadIdx];

        SpenderEmissionLoop::RateSampler rates{budget(), state, frame,
                                               rulesFrom(behavior_)};
        rates.dailyMultipliers(dailyMultipliers).ledgerView(gate);

        SpenderEmissionLoop::PaymentEmitter payments{market(), routingSnapshot,
                                                     factory(), gate};
        SpenderEmissionLoop loop{population, rates, payments};

        loop.run(range.begin, range.end, std::span<random::Rng>{spenderRngs_},
                 scratch.txns, scratch.postings);
      };
  pool_->run(emitBody);

  mergeThreadTxns(state);
}

void SpenderEmissionDriver::finish(RunState &state) { mergeThreadTxns(state); }

} // namespace PhantomLedger::activity::spending::simulator
