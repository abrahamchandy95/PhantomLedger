#include "phantomledger/spending/simulator/driver.hpp"

#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/simulator/payday_index.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/spending/spenders/prepare.hpp"
#include "phantomledger/spending/spenders/targets.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace PhantomLedger::spending::simulator {

namespace {

constexpr double kTxnReserveSlack = 1.05;

void sortChronological(std::vector<transactions::Transaction> &txns) {
  std::sort(
      txns.begin(), txns.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
}

void resolvePersonPrimaryIdx(
    const market::Market &market,
    const ::PhantomLedger::clearing::Ledger &ledger,
    std::vector<::PhantomLedger::clearing::Ledger::Index> &out) {
  const auto &pop = market.population();
  const auto count = pop.count();
  out.assign(count, ::PhantomLedger::clearing::Ledger::invalid);

  for (std::uint32_t i = 0; i < count; ++i) {
    const auto person = static_cast<entity::PersonId>(i + 1);
    const auto key = pop.primary(person);
    if (entity::valid(key)) {
      out[i] = ledger.findAccount(key);
    }
  }
}

void resolveMerchantCounterpartyIdx(
    const market::Market &market,
    const ::PhantomLedger::clearing::Ledger &ledger,
    std::vector<::PhantomLedger::clearing::Ledger::Index> &out) {
  const auto *catalog = market.commerce().catalog();
  if (catalog == nullptr) {
    out.clear();
    return;
  }

  out.assign(catalog->records.size(),
             ::PhantomLedger::clearing::Ledger::invalid);

  for (std::size_t i = 0; i < catalog->records.size(); ++i) {
    out[i] = ledger.findAccount(catalog->records[i].counterpartyId);
  }
}

/// Render a non-negative integer to decimal in a small stack buffer.
/// Used for the per-thread RNG seed tag.
[[nodiscard]] std::string_view renderUInt(std::array<char, 16> &buf,
                                          std::uint32_t value) noexcept {
  auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
  (void)ec;
  return std::string_view(buf.data(),
                          static_cast<std::size_t>(ptr - buf.data()));
}

} // namespace

Simulator::Simulator(const market::Market &market, Engine &engine,
                     const obligations::Snapshot &obligations,
                     const SimulatorConfig &config)
    : market_(market), engine_(engine), obligations_(obligations),
      config_(config), cohort_(market.population().count()) {
  const auto n = market.population().count();

  sensitivities_.resize(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    const auto person = static_cast<entity::PersonId>(i + 1);
    sensitivities_[i] = market.population().object(person).payday.sensitivity;
  }

  dailyMultBuffer_.resize(n);
}

RunPlan Simulator::buildPlan() const {
  RunPlan plan{};

  plan.population.spenders =
      spenders::prepareSpenders(market_, obligations_, engine_.ledger);

  const std::uint32_t activeSpenders = plan.population.activeCount();

  plan.budget.targetTotalTxns = spenders::totalTargetTxns(
      config_.txnsPerMonth, activeSpenders, market_.bounds().days);

  plan.budget.totalPersonDays =
      static_cast<std::uint64_t>(activeSpenders) * market_.bounds().days;

  plan.baseLedger.txns = obligations_.baseTxns;
  plan.population.sensitivities = std::span<const double>(sensitivities_);

  plan.payday.index =
      PaydayIndex::build(market_.population().paydays(), market_.bounds().days);

  if (config_.personDailyLimit > 0) {
    plan.budget.personLimit = config_.personDailyLimit;
  }

  plan.routing.channelCdf = routing::buildChannelCdf(
      config_.channels.merchantP, config_.channels.billsP,
      config_.channels.p2pP, config_.channels.externalUnknownP);

  plan.routing.policy = routing::Policy{
      .preferBillersP = config_.preferBillersP,
      .maxRetries = config_.picking.maxRetries,
  };

  plan.activity.baseExploreP = config_.baseExploreP;
  plan.activity.dayShockShape = config_.dayShockShape;

  if (engine_.ledger != nullptr) {
    resolvePersonPrimaryIdx(market_, *engine_.ledger,
                            plan.routing.personPrimaryIdx);

    resolveMerchantCounterpartyIdx(market_, *engine_.ledger,
                                   plan.routing.merchantCounterpartyIdx);

    const entity::Key externalKey =
        entity::makeKey(entity::Role::merchant, entity::Bank::external, 1u);

    plan.routing.externalUnknownIdx = engine_.ledger->findAccount(externalKey);
  }

  return plan;
}

void Simulator::prepareThreadStates() {
  threadStates_.clear();
  if (engine_.threadCount <= 1) {
    return;
  }

  if (engine_.rngFactory == nullptr) {
    throw std::runtime_error(
        "spending::Simulator: threadCount > 1 requires engine.rngFactory");
  }
  if (engine_.factory == nullptr) {
    throw std::runtime_error(
        "spending::Simulator: threadCount > 1 requires engine.factory "
        "(per-thread Factory is built via factory->rebound)");
  }

  threadStates_.reserve(engine_.threadCount);

  // Pre-reserve each thread's txn buffer to avoid reallocations during
  // the run. Conservative estimate: each thread will produce roughly
  // (target / threadCount) txns. Add slack for noise.
  const auto perThreadReserve = static_cast<std::size_t>(
      (static_cast<double>(market_.bounds().days) *
       (static_cast<double>(market_.population().count()) / 30.0) *
       config_.txnsPerMonth * kTxnReserveSlack) /
      static_cast<double>(engine_.threadCount));

  std::array<char, 16> idBuf{};
  for (std::uint32_t t = 0; t < engine_.threadCount; ++t) {
    const auto idStr = renderUInt(idBuf, t);
    auto threadRng = engine_.rngFactory->rng({"spending_thread", idStr});
    threadStates_.emplace_back(std::move(threadRng));
    threadStates_.back().txns.reserve(perThreadReserve);
  }
}

void Simulator::prepareLockArray() {
  if (engine_.threadCount <= 1 || engine_.ledger == nullptr) {
    lockArray_.resize(0);
    return;
  }

  lockArray_.resize(static_cast<std::size_t>(engine_.ledger->size()));
}

void Simulator::mergeThreadTxns(RunState &state) {
  if (threadStates_.empty()) {
    return;
  }

  // Sum first so we can reserve once and avoid repeated growth on the
  // shared vector.
  std::size_t total = state.txns().size();
  for (const auto &ts : threadStates_) {
    total += ts.txns.size();
  }

  state.txns().reserve(total);
  for (auto &ts : threadStates_) {
    state.txns().insert(state.txns().end(),
                        std::make_move_iterator(ts.txns.begin()),
                        std::make_move_iterator(ts.txns.end()));
    ts.txns.clear();
  }
}

std::vector<transactions::Transaction> Simulator::run() {
  if (market_.bounds().days == 0) {
    return {};
  }

  RunPlan plan = buildPlan();

  const std::size_t reserveCapacity =
      engine_.threadCount <= 1
          ? static_cast<std::size_t>(plan.budget.targetTotalTxns *
                                     kTxnReserveSlack)
          : 0;

  RunState state(market_.population().count(), reserveCapacity,
                 plan.budget.totalPersonDays, plan.budget.targetTotalTxns);
  prepareThreadStates();
  prepareLockArray();

  for (std::uint32_t dayIndex = 0; dayIndex < market_.bounds().days;
       ++dayIndex) {
    runDay(plan, state, dayIndex);
  }

  // Drain per-thread buffers and sort the result chronologically.
  mergeThreadTxns(state);

  auto txns = std::move(state.txns());
  sortChronological(txns);
  return txns;
}

} // namespace PhantomLedger::spending::simulator
