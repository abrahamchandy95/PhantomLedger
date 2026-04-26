#include "phantomledger/spending/simulator/day.hpp"

#include "phantomledger/math/counts.hpp"
#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/math/timing.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/spending/actors/counts.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/event.hpp"
#include "phantomledger/spending/actors/explore.hpp"
#include "phantomledger/spending/clearing/parallel_ledger_view.hpp"
#include "phantomledger/spending/dynamics/monthly/evolution.hpp"
#include "phantomledger/spending/liquidity/multiplier.hpp"
#include "phantomledger/spending/liquidity/snapshot.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/dispatch.hpp"
#include "phantomledger/spending/routing/emission_result.hpp"
#include "phantomledger/spending/simulator/state.hpp"
#include "phantomledger/spending/simulator/thread_runner.hpp"
#include "phantomledger/spending/spenders/targets.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"

#include <algorithm>
#include <cstdint>

namespace PhantomLedger::spending::simulator {

namespace {

[[nodiscard]] bool isMonthBoundary(std::uint32_t dayIndex,
                                   time::TimePoint windowStart) noexcept {
  if (dayIndex == 0) {
    return false;
  }
  const auto prev = time::addDays(windowStart, static_cast<int>(dayIndex) - 1);
  const auto curr = time::addDays(windowStart, static_cast<int>(dayIndex));
  const auto prevCal = time::toCalendarDate(prev);
  const auto currCal = time::toCalendarDate(curr);
  return currCal.month != prevCal.month || currCal.year != prevCal.year;
}

/// Bundle of read-only inputs the per-spender loop needs. Same in
/// serial and parallel modes — only the (rng, factory, txns) trio
/// changes per worker.
struct DayContext {
  const market::Market &market;
  const RunPlan &plan;
  RunState &sharedState;
  const actors::Day &day;
  double weekdayMult;
  double seasonalMult;
  std::span<const double> dailyMultBuffer;
  const config::BurstBehavior &burst;
  const config::ExplorationHabits &exploration;
  const config::LiquidityConstraints &liquidity;
  const routing::ResolvedAccounts &resolved;
  ParallelLedgerView ledgerView;
};

double availableCashFor(ParallelLedgerView &ledgerView,
                        const spenders::PreparedSpender &prepared) {
  if (ledgerView.empty()) {
    return prepared.initialCash;
  }
  const auto idx = prepared.spender.depositAccountIdx;
  if (idx == ::PhantomLedger::clearing::Ledger::invalid) {
    return prepared.initialCash;
  }
  return ledgerView.availableCash(idx);
}

bool tryEmitWithLedger(ParallelLedgerView &ledgerView,
                       const routing::EmissionResult &er) {
  return ledgerView
      .transfer(er.srcIdx, er.dstIdx, er.transaction.amount,
                er.transaction.session.channel)
      .accepted();
}

/// Run one chunk of the per-spender loop. Same body for serial and
/// parallel: the only things that differ are the `Rng` instance, the
/// `Factory` instance, and the destination `txns` vector. Everything
/// else comes from `ctx`.
void runSpenderRange(DayContext &ctx, std::size_t begin, std::size_t end,
                     random::Rng &rng, const transactions::Factory &factory,
                     std::vector<transactions::Transaction> &outTxns) {
  const double explorationFloor = ctx.liquidity.explorationFloor;
  const auto &spenders = ctx.plan.preparedSpenders;

  for (std::size_t i = begin; i < end; ++i) {
    const auto &prepared = spenders[i];
    const auto personIndex = prepared.spender.personIndex;
    const auto &spender = prepared.spender;

    // a. Liquidity snapshot — uses the pre-resolved deposit index.
    const double availableCash = availableCashFor(ctx.ledgerView, prepared);
    const liquidity::Snapshot liqSnap{
        .daysSincePayday = ctx.sharedState.daysSincePayday(personIndex),
        .paycheckSensitivity = prepared.paycheckSensitivity,
        .availableCash = availableCash,
        .baselineCash = prepared.baselineCash,
        .fixedMonthlyBurden = prepared.fixedBurden,
    };
    const double liquidityMult = liquidity::multiplier(ctx.liquidity, liqSnap);

    // b. Combined dynamics multiplier (already fused).
    const double combinedMult =
        ctx.dailyMultBuffer[personIndex] * ctx.seasonalMult;

    // c. Plan-budget targeting using the running counters. In parallel
    // mode these are atomic; reads are slightly stale across threads,
    // which biases the per-day target rate by fractions of a percent —
    // well within the 5% reserve already baked into the target.
    const std::uint64_t remainingPersonDays =
        std::max<std::uint64_t>(1u, ctx.sharedState.remainingPersonDays());
    const double targetRealizedPerDay =
        ctx.sharedState.remainingTargetTxns() /
        static_cast<double>(remainingPersonDays);

    const double latentBaseRate = spenders::baseRateForTarget(
        spender, ctx.day.shock, ctx.weekdayMult, targetRealizedPerDay,
        combinedMult, liquidityMult);

    // d. Sample the integer count.
    const auto txnCount = actors::sampleTxnCount(
        rng, spender, ctx.day, latentBaseRate, ctx.weekdayMult,
        ctx.plan.personLimit, combinedMult, liquidityMult);

    ctx.sharedState.consumeOnePersonDay();

    if (txnCount == 0) {
      continue;
    }

    // e. exploreP with liquidity-cubed dampening.
    double exploreP = actors::calculateExploreP(
        ctx.plan.baseExploreP, ctx.exploration, ctx.burst, spender, ctx.day);
    const double cubed =
        std::clamp(liquidityMult * liquidityMult * liquidityMult, 0.0, 1.0);
    exploreP *= std::max(explorationFloor, cubed);

    // f. Attempt loop.
    std::uint32_t accepted = 0;
    std::uint32_t attemptBudget = txnCount * 4u;

    while (accepted < txnCount && attemptBudget > 0) {
      --attemptBudget;

      const std::int32_t offsetSec =
          math::timing::sampleOffset(rng, spender.timing);

      actors::Event event{};
      event.spender = &spender;
      event.factory = &factory;
      event.ts = ctx.day.start + time::Seconds{offsetSec};
      event.exploreP = exploreP;

      const routing::Slot slot =
          routing::pickSlot(ctx.plan.channelCdf, rng.nextDouble());

      auto maybeResult = routing::routeTxn(
          rng, ctx.market, ctx.plan.routePolicy, ctx.resolved, slot, event);
      if (!maybeResult.has_value()) {
        continue;
      }

      // Index-based ledger transfer through the parallel-aware view.
      // In serial mode this is unsynchronised (matches Phase 2). In
      // parallel mode the view acquires the per-account spinlock(s).
      if (!tryEmitWithLedger(ctx.ledgerView, *maybeResult)) {
        continue;
      }

      outTxns.push_back(std::move(maybeResult->transaction));
      ++accepted;
    }

    ctx.sharedState.recordAccepted(accepted);
  }
}

} // namespace

void runDay(const market::Market &market, Engine &engine, const RunPlan &plan,
            RunState &state, dynamics::population::Cohort &cohort,
            const dynamics::Config &dynamicsCfg,
            const config::BurstBehavior &burst,
            const config::ExplorationHabits &exploration,
            const config::LiquidityConstraints &liquidity,
            const math::seasonal::Config &seasonal,
            std::span<double> dailyMultBuffer, std::uint32_t dayIndex,
            std::span<ThreadLocalState> threadStates,
            primitives::concurrent::AccountLockArray *lockArray) {
  random::Rng &dynRng = *engine.rng;

  // --- 1. Month-boundary evolution (serial) -------------------------------
  if (isMonthBoundary(dayIndex, market.bounds().startDate)) {
    auto &mutCommerce = const_cast<market::Market &>(market).commerceMutable();
    dynamics::monthly::evolveAll(
        dynRng, dynamicsCfg.evolution, mutCommerce, mutCommerce.merchCdf(),
        static_cast<std::uint32_t>(mutCommerce.merchCdf().size()),
        market.population().count());
  }

  // --- 2. Day frame + per-day scalar multipliers (serial) -----------------
  const auto day = actors::buildDay(market.bounds().startDate,
                                    plan.dayShockShape, dynRng, dayIndex);
  const double seasonalMult =
      math::seasonal::dailyMultiplier(day.start, seasonal);
  const double weekdayMult = math::counts::weekdayMultiplier(day.start);

  // --- 3. Advance the screening ledger to today's start (serial) ----------
  const auto newBaseIdx = clearing::advanceBookThrough(
      engine.ledger, plan.baseTxns, state.baseIdx(),
      time::toEpochSeconds(day.start), /*inclusive=*/false);
  state.setBaseIdx(newBaseIdx);

  // --- 4. Days-since-payday update (serial vector sweep) ------------------
  state.bumpAllDaysSincePayday();
  const auto paydayPersons = plan.paydayIndex.personsOn(dayIndex);
  for (const auto idx : paydayPersons) {
    state.resetDaysSincePayday(idx);
  }

  // --- 5. Batched dynamics advance (serial; uses engine.rng) --------------
  cohort.advanceAll(dynRng, dynamicsCfg, paydayPersons, plan.sensitivities,
                    dailyMultBuffer);

  // --- 6. Per-spender attempt loop ----------------------------------------
  const routing::ResolvedAccounts resolved{
      .personPrimaryIdx =
          std::span<const ::PhantomLedger::clearing::Ledger::Index>(
              plan.personPrimaryIdx),
      .merchantCounterpartyIdx =
          std::span<const ::PhantomLedger::clearing::Ledger::Index>(
              plan.merchantCounterpartyIdx),
      .externalUnknownIdx = plan.externalUnknownIdx,
  };

  DayContext ctx{
      .market = market,
      .plan = plan,
      .sharedState = state,
      .day = day,
      .weekdayMult = weekdayMult,
      .seasonalMult = seasonalMult,
      .dailyMultBuffer = std::span<const double>(dailyMultBuffer.data(),
                                                 dailyMultBuffer.size()),
      .burst = burst,
      .exploration = exploration,
      .liquidity = liquidity,
      .resolved = resolved,
      .ledgerView = ParallelLedgerView{engine.ledger, lockArray},
  };

  const std::size_t spenderCount = plan.preparedSpenders.size();

  if (engine.threadCount <= 1) {
    // Serial path — uses engine.rng and engine.factory directly,
    // pushes straight into the shared state's txn buffer. Bit-identical
    // to the Phase 2 behaviour when threadCount == 1.
    runSpenderRange(ctx, 0, spenderCount, *engine.rng, *engine.factory,
                    state.txns());
    return;
  }

  // Parallel path — each worker thread handles a contiguous chunk of
  // spenders using its own RNG, its own rebound Factory, and its own
  // txn buffer. The buffers are merged at end-of-run in the driver.
  runParallel(engine.threadCount, [&](std::uint32_t threadIdx) {
    const auto range =
        partitionRange(spenderCount, engine.threadCount, threadIdx);
    if (range.size() == 0) {
      return;
    }
    auto &ts = threadStates[threadIdx];
    const auto threadFactory = engine.factory->rebound(ts.rng);
    runSpenderRange(ctx, range.begin, range.end, ts.rng, threadFactory,
                    ts.txns);
  });
}

} // namespace PhantomLedger::spending::simulator
