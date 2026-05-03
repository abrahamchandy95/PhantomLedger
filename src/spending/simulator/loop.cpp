#include "phantomledger/spending/simulator/loop.hpp"

#include "phantomledger/math/timing.hpp"
#include "phantomledger/spending/actors/counts.hpp"
#include "phantomledger/spending/actors/event.hpp"
#include "phantomledger/spending/actors/explore.hpp"
#include "phantomledger/spending/liquidity/multiplier.hpp"
#include "phantomledger/spending/liquidity/snapshot.hpp"
#include "phantomledger/spending/routing/channel.hpp"
#include "phantomledger/spending/routing/dispatch.hpp"
#include "phantomledger/spending/spenders/targets.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace PhantomLedger::spending::simulator {

SpenderEmissionLoop::SpenderEmissionLoop(
    const market::Market &market, const RunPlan &plan, RunState &state,
    const actors::DayFrame &frame, std::span<const double> dailyMultipliers,
    SpenderEmissionPolicy policy, const routing::ResolvedAccounts &resolved,
    ParallelLedgerView ledgerView) noexcept
    : market_(market), plan_(plan), state_(state), frame_(frame),
      dailyMultipliers_(dailyMultipliers), policy_(policy), resolved_(resolved),
      ledgerView_(ledgerView) {}

double SpenderEmissionLoop::availableCashFor(
    const spenders::PreparedSpender &prepared) {
  if (ledgerView_.empty()) {
    return prepared.initialCash;
  }

  const auto idx = prepared.spender.depositAccountIdx;
  if (idx == ::PhantomLedger::clearing::Ledger::invalid) {
    return prepared.initialCash;
  }

  return ledgerView_.availableCash(idx);
}

double SpenderEmissionLoop::liquidityMultiplierFor(
    const spenders::PreparedSpender &prepared) {
  const auto personIndex = prepared.spender.personIndex;

  const liquidity::Snapshot snapshot{
      .daysSincePayday = state_.daysSincePayday(personIndex),
      .paycheckSensitivity = prepared.paycheckSensitivity,
      .availableCash = availableCashFor(prepared),
      .baselineCash = prepared.baselineCash,
      .fixedMonthlyBurden = prepared.fixedBurden,
  };

  return liquidity::multiplier(policy_.liquidity, snapshot);
}

double
SpenderEmissionLoop::combinedMultiplierFor(std::uint32_t personIndex) const {
  return dailyMultipliers_[personIndex] * frame_.seasonalMult;
}

double SpenderEmissionLoop::latentBaseRateFor(const actors::Spender &spender,
                                              double combinedMult,
                                              double liquidityMult) const {
  const std::uint64_t remainingPersonDays =
      std::max<std::uint64_t>(std::uint64_t{1}, state_.remainingPersonDays());

  const double targetRealizedPerDay =
      state_.remainingTargetTxns() / static_cast<double>(remainingPersonDays);

  return spenders::baseRateForTarget(spender, frame_.day.shock,
                                     frame_.weekdayMult, targetRealizedPerDay,
                                     combinedMult, liquidityMult);
}

std::uint32_t SpenderEmissionLoop::transactionCountFor(
    random::Rng &rng, const actors::Spender &spender, double latentBaseRate,
    double combinedMult, double liquidityMult) const {
  return actors::sampleTransactionCount(
      rng, spender, frame_,
      actors::RatePieces{
          .baseRate = latentBaseRate,
          .weekdayMult = frame_.weekdayMult,
          .dynamicsMultiplier = combinedMult,
          .liquidityMultiplier = liquidityMult,
      },
      plan_.budget.personLimit);
}

double
SpenderEmissionLoop::exploreProbabilityFor(const actors::Spender &spender,
                                           double liquidityMult) const {
  double exploreP = actors::calculateExploreP(
      policy_.baseExploreP, policy_.exploration, spender, frame_.day);

  const double cubed =
      std::clamp(liquidityMult * liquidityMult * liquidityMult, 0.0, 1.0);

  exploreP *= std::max(policy_.liquidity.explorationFloor, cubed);

  return exploreP;
}

bool SpenderEmissionLoop::tryEmit(const routing::EmissionResult &result) {
  return ledgerView_
      .transfer(result.srcIdx, result.dstIdx, result.transaction.amount,
                result.transaction.session.channel)
      .accepted();
}

void SpenderEmissionLoop::run(std::size_t begin, std::size_t end,
                              random::Rng &rng,
                              const transactions::Factory &factory,
                              std::vector<transactions::Transaction> &outTxns) {
  const auto &spenders = plan_.population.spenders;

  for (std::size_t i = begin; i < end; ++i) {
    const auto &prepared = spenders[i];
    const auto &spender = prepared.spender;
    const auto personIndex = spender.personIndex;

    const double liquidityMult = liquidityMultiplierFor(prepared);
    const double combinedMult = combinedMultiplierFor(personIndex);

    const double latentBaseRate =
        latentBaseRateFor(spender, combinedMult, liquidityMult);

    const auto txnCount = transactionCountFor(rng, spender, latentBaseRate,
                                              combinedMult, liquidityMult);

    state_.consumeOnePersonDay();

    if (txnCount == 0) {
      continue;
    }

    const double exploreP = exploreProbabilityFor(spender, liquidityMult);

    std::uint32_t accepted = 0;
    std::uint32_t attemptBudget = txnCount * 4u;

    while (accepted < txnCount && attemptBudget > 0) {
      --attemptBudget;

      const std::int32_t offsetSec =
          math::timing::sampleOffset(rng, spender.timing);

      actors::Event event{};
      event.spender = &spender;
      event.factory = &factory;
      event.ts = frame_.day.start + time::Seconds{offsetSec};
      event.exploreP = exploreP;

      const routing::Slot slot =
          routing::pickSlot(plan_.routing.channelCdf, rng.nextDouble());

      auto maybeResult = routing::routeTxn(
          rng, market_, plan_.routing.paymentRules, resolved_, slot, event);

      if (!maybeResult.has_value()) {
        continue;
      }

      if (!tryEmit(*maybeResult)) {
        continue;
      }

      outTxns.push_back(std::move(maybeResult->transaction));
      ++accepted;
    }

    state_.recordAccepted(accepted);
  }
}

} // namespace PhantomLedger::spending::simulator
