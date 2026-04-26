#include "phantomledger/spending/simulator/day.hpp"

#include "phantomledger/math/seasonal.hpp"
#include "phantomledger/math/timing.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/spending/actors/counts.hpp"
#include "phantomledger/spending/actors/day.hpp"
#include "phantomledger/spending/actors/event.hpp"
#include "phantomledger/spending/actors/explore.hpp"
#include "phantomledger/spending/dynamics/daily/compose.hpp"
#include "phantomledger/spending/dynamics/monthly/evolution.hpp"
#include "phantomledger/spending/liquidity/multiplier.hpp"
#include "phantomledger/spending/liquidity/snapshot.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/dispatch.hpp"
#include "phantomledger/spending/spenders/targets.hpp"
#include "phantomledger/transactions/clearing/screening.hpp"

#include <algorithm>
#include <cmath>
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

void buildPaydayIndices(const RunPlan &plan, std::uint32_t dayIndex,
                        std::vector<std::uint32_t> &out) {
  out.clear();
  const auto &spenders = plan.preparedSpenders;
  for (std::size_t i = 0; i < spenders.size(); ++i) {
    const auto &paydays = spenders[i].paydays;
    if (std::binary_search(paydays.begin(), paydays.end(), dayIndex)) {
      out.push_back(spenders[i].spender.personIndex);
    }
  }
}

double availableCashFor(const market::Market & /*market*/,
                        clearing::Ledger *ledger,
                        const spenders::PreparedSpender &prepared) {
  if (ledger == nullptr) {
    return prepared.initialCash;
  }
  return ledger->availableCash(prepared.spender.depositAccount);
}

bool tryEmitWithLedger(clearing::Ledger *ledger,
                       const transactions::Transaction &txn) {
  if (ledger == nullptr) {
    return true;
  }
  return ledger
      ->transfer(txn.source, txn.target, txn.amount, txn.session.channel)
      .accepted();
}

} // namespace

void runDay(const market::Market &market, Engine &engine, const RunPlan &plan,
            RunState &state, dynamics::population::Cohort &cohort,
            const dynamics::Config &dynamicsCfg,
            const config::BurstBehavior &burst,
            const config::ExplorationHabits &exploration,
            const config::LiquidityConstraints &liquidity,
            const math::seasonal::Config &seasonal, std::uint32_t dayIndex) {
  random::Rng &rng = *engine.rng;

  // --- 1. Month-boundary evolution --------------------------------
  if (isMonthBoundary(dayIndex, market.bounds().startDate)) {
    // Note: const_cast only acceptable here because the simulator
    // owns the run and the kernel is the only writer to commerce
    // during the run loop. Marked clearly to flag the scope.
    auto &mutCommerce = const_cast<market::Market &>(market).commerceMutable();
    dynamics::monthly::evolveAll(
        rng, dynamicsCfg.evolution, mutCommerce, mutCommerce.merchCdf(),
        static_cast<std::uint32_t>(mutCommerce.merchCdf().size()),
        market.population().count());
  }

  // --- 2. Build the Day frame -------------------------------------
  const auto day = actors::buildDay(market.bounds().startDate,
                                    plan.dayShockShape, rng, dayIndex);
  // `dailyMultiplier` accepts a TimePoint directly; the previous
  // `monthlyMultiplier(time::month(...))` form fabricated a missing
  // helper.
  const double seasonalMult =
      math::seasonal::dailyMultiplier(day.start, seasonal);

  // --- 3. Advance the screening ledger to today's start -----------
  const auto newBaseIdx = clearing::advanceBookThrough(
      engine.ledger, plan.baseTxns, state.baseIdx(),
      time::toEpochSeconds(day.start), /*inclusive=*/false);
  state.setBaseIdx(newBaseIdx);

  // --- 4. Batched dynamics advance --------------------------------
  static thread_local std::vector<std::uint32_t> paydayIdxScratch;
  buildPaydayIndices(plan, dayIndex, paydayIdxScratch);

  std::vector<double> momentumMult(market.population().count());
  std::vector<double> dormancyMult(market.population().count());
  std::vector<double> paycheckMult(market.population().count());

  cohort.advanceAll(rng, dynamicsCfg, paydayIdxScratch, plan.sensitivities,
                    momentumMult, dormancyMult, paycheckMult);

  // --- 5. Per-spender attempt loop --------------------------------
  for (const auto &prepared : plan.preparedSpenders) {
    const auto personIndex = prepared.spender.personIndex;

    // a. days_since_payday update.
    const bool isPayday = std::binary_search(prepared.paydays.begin(),
                                             prepared.paydays.end(), dayIndex);
    if (isPayday) {
      state.resetDaysSincePayday(personIndex);
    } else {
      state.incrementDaysSincePayday(personIndex);
    }

    // b. liquidity multiplier.
    const double availableCash =
        availableCashFor(market, engine.ledger, prepared);
    const liquidity::Snapshot liqSnap{
        .daysSincePayday = state.daysSincePayday(personIndex),
        .paycheckSensitivity = prepared.paycheckSensitivity,
        .availableCash = availableCash,
        .baselineCash = prepared.baselineCash,
        .fixedMonthlyBurden = prepared.fixedBurden,
    };
    const double liquidityMult = liquidity::multiplier(liquidity, liqSnap);

    // c. combined dynamics multiplier.
    const double combinedMult = momentumMult[personIndex] *
                                dormancyMult[personIndex] *
                                paycheckMult[personIndex] * seasonalMult;

    const std::uint64_t remainingPersonDays = std::max<std::uint64_t>(
        1u, plan.totalPersonDays - state.processedPersonDays());
    const double remainingTargetTxns = std::max(
        0.0, plan.targetTotalTxns - static_cast<double>(state.txns().size()));
    const double targetRealizedPerDay =
        remainingTargetTxns / static_cast<double>(remainingPersonDays);

    const double latentBaseRate = spenders::baseRateForTarget(
        prepared.spender, day.shock, day.start, targetRealizedPerDay,
        combinedMult, liquidityMult);

    // d. sample count.
    const auto txnCount =
        actors::sampleTxnCount(rng, prepared.spender, day, latentBaseRate,
                               plan.personLimit, combinedMult, liquidityMult);

    state.incrementProcessedPersonDays();

    if (txnCount == 0) {
      continue;
    }

    // e. explore_p with liquidity-cubed dampening.
    double exploreP = actors::calculateExploreP(plan.baseExploreP, exploration,
                                                burst, prepared.spender, day);
    const double cubed =
        std::clamp(liquidityMult * liquidityMult * liquidityMult, 0.0, 1.0);
    exploreP *= std::max(liquidity.explorationFloor, cubed);

    // f. attempt loop.
    std::uint32_t accepted = 0;
    std::uint32_t attemptBudget = std::max(txnCount, txnCount * 4u);

    while (accepted < txnCount && attemptBudget > 0) {
      --attemptBudget;

      // `sampleOffset` reads its CDF from `personas::Timing` directly.
      // The previous three-arg form was based on a `Profiles` type
      // that does not exist.
      const std::int32_t offsetSec = math::timing::sampleOffset(
          rng, prepared.spender.persona->archetype.timing);

      actors::Event event{};
      event.spender = &prepared.spender;
      event.factory = engine.factory;
      event.ts = day.start + time::Seconds{offsetSec};
      event.exploreP = exploreP;

      const routing::Slot slot =
          routing::pickSlot(plan.channelCdf, rng.nextDouble());

      auto maybeTxn =
          routing::routeTxn(rng, market, plan.routePolicy, slot, event);
      if (!maybeTxn.has_value()) {
        continue;
      }

      if (!tryEmitWithLedger(engine.ledger, *maybeTxn)) {
        continue;
      }

      state.txns().push_back(*maybeTxn);
      ++accepted;
    }
  }
}

} // namespace PhantomLedger::spending::simulator
