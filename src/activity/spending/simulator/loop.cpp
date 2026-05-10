#include "phantomledger/activity/spending/simulator/loop.hpp"

#include "phantomledger/activity/spending/actors/counts.hpp"
#include "phantomledger/activity/spending/actors/event.hpp"
#include "phantomledger/activity/spending/actors/explore.hpp"
#include "phantomledger/activity/spending/liquidity/multiplier.hpp"
#include "phantomledger/activity/spending/liquidity/snapshot.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/routing/router.hpp"
#include "phantomledger/activity/spending/spenders/targets.hpp"
#include "phantomledger/math/timing.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace PhantomLedger::spending::simulator {

SpenderEmissionLoop::RateSampler::RateSampler(const PreparedRun::Budget &budget,
                                              RunState &state,
                                              const actors::DayFrame &frame,
                                              Rules rules) noexcept
    : budget_(budget), state_(state), frame_(frame), rules_(rules) {}

SpenderEmissionLoop::RateSampler &
SpenderEmissionLoop::RateSampler::dailyMultipliers(
    std::span<const double> value) noexcept {
  dailyMultipliers_ = value;
  return *this;
}

SpenderEmissionLoop::RateSampler &SpenderEmissionLoop::RateSampler::ledgerView(
    ParallelLedgerView value) noexcept {
  ledgerView_ = value;
  return *this;
}

double SpenderEmissionLoop::RateSampler::availableCashFor(
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

double SpenderEmissionLoop::RateSampler::liquidityMultiplierFor(
    const spenders::PreparedSpender &prepared) {
  const auto personIndex = prepared.spender.personIndex;

  const liquidity::Snapshot snapshot{
      .daysSincePayday = state_.daysSincePayday(personIndex),
      .paycheckSensitivity = prepared.paycheckSensitivity,
      .availableCash = availableCashFor(prepared),
      .baselineCash = prepared.baselineCash,
      .fixedMonthlyBurden = prepared.fixedBurden,
  };

  return liquidity::multiplier(rules_.liquidity, snapshot);
}

double SpenderEmissionLoop::RateSampler::combinedMultiplierFor(
    std::uint32_t personIndex) const {
  return dailyMultipliers_[personIndex] * frame_.seasonalMult;
}

double SpenderEmissionLoop::RateSampler::latentBaseRateFor(
    const actors::Spender &spender, DailyMultipliers mults) const {
  const std::uint64_t remainingPersonDays =
      std::max<std::uint64_t>(std::uint64_t{1}, state_.remainingPersonDays());

  const double targetRealizedPerDay =
      state_.remainingTargetTxns() / static_cast<double>(remainingPersonDays);

  return spenders::baseRateForTarget(spender,
                                     actors::RatePieces{
                                         .weekdayMult = frame_.weekdayMult,
                                         .dynamicsMultiplier = mults.combined,
                                         .liquidityMultiplier = mults.liquidity,
                                         .dayShock = frame_.day.shock,
                                     },
                                     targetRealizedPerDay);
}

std::uint32_t SpenderEmissionLoop::RateSampler::transactionCountFor(
    random::Rng &rng, const actors::Spender &spender, double latentBaseRate,
    DailyMultipliers mults) const {
  return actors::sampleTransactionCount(
      rng, spender,
      actors::RatePieces{
          .baseRate = latentBaseRate,
          .weekdayMult = frame_.weekdayMult,
          .dynamicsMultiplier = mults.combined,
          .liquidityMultiplier = mults.liquidity,
          .dayShock = frame_.day.shock,
      },
      budget_.personLimit);
}

double SpenderEmissionLoop::RateSampler::exploreProbabilityFor(
    const actors::Spender &spender, double liquidityMult) const {
  double exploreP = actors::calculateExploreP(
      rules_.baseExploreP, rules_.exploration, spender, frame_.day);

  const double cubed =
      std::clamp(liquidityMult * liquidityMult * liquidityMult, 0.0, 1.0);

  exploreP *= std::max(rules_.liquidity.explorationFloor, cubed);

  return exploreP;
}

::PhantomLedger::time::TimePoint
SpenderEmissionLoop::RateSampler::timestampAtOffset(
    std::int32_t offsetSec) const noexcept {
  return frame_.day.start + ::PhantomLedger::time::Seconds{offsetSec};
}

void SpenderEmissionLoop::RateSampler::consumeOnePersonDay() noexcept {
  state_.consumeOnePersonDay();
}

void SpenderEmissionLoop::RateSampler::recordAccepted(
    std::uint32_t count) noexcept {
  state_.recordAccepted(count);
}

SpenderEmissionLoop::PaymentEmitter::PaymentEmitter(
    const market::Market &market, const PreparedRun::Routing &routing,
    const transactions::Factory &factory,
    ParallelLedgerView ledgerView) noexcept
    : market_(market), routing_(routing), factory_(factory),
      resolved_(routing.resolvedAccounts()), ledgerView_(ledgerView) {}

bool SpenderEmissionLoop::PaymentEmitter::accept(
    const routing::EmissionResult &result) {
  return ledgerView_
      .transfer(result.srcIdx, result.dstIdx, result.draft.amount,
                result.draft.channel)
      .accepted();
}

std::optional<transactions::Transaction>
SpenderEmissionLoop::PaymentEmitter::tryEmit(random::Rng &rng,
                                             const actors::Event &event) {
  const routing::Slot slot =
      routing::pickSlot(routing_.channelCdf, rng.nextDouble());

  routing::PaymentRouter router{rng, market_, routing_.paymentRules, resolved_};
  auto maybeResult = router.route(slot, event);

  if (!maybeResult.has_value()) {
    return std::nullopt;
  }

  auto txn = factory_.make(maybeResult->draft);

  if (!accept(*maybeResult)) {
    return std::nullopt;
  }

  return txn;
}

SpenderEmissionLoop::SpenderEmissionLoop(
    const PreparedRun::Population &population, RateSampler &rates,
    PaymentEmitter &payments) noexcept
    : population_(population), rates_(rates), payments_(payments) {}

void SpenderEmissionLoop::run(std::size_t begin, std::size_t end,
                              random::Rng &rng,
                              std::vector<transactions::Transaction> &outTxns) {
  const auto &spenders = population_.spenders;

  for (std::size_t i = begin; i < end; ++i) {
    const auto &prepared = spenders[i];
    const auto &spender = prepared.spender;
    const auto personIndex = spender.personIndex;

    const double liquidityMult = rates_.liquidityMultiplierFor(prepared);
    const double combinedMult = rates_.combinedMultiplierFor(personIndex);
    const RateSampler::DailyMultipliers mults{.combined = combinedMult,
                                              .liquidity = liquidityMult};

    const double latentBaseRate = rates_.latentBaseRateFor(spender, mults);

    const auto txnCount =
        rates_.transactionCountFor(rng, spender, latentBaseRate, mults);

    rates_.consumeOnePersonDay();

    if (txnCount == 0) {
      continue;
    }

    const double exploreP =
        rates_.exploreProbabilityFor(spender, liquidityMult);

    std::uint32_t accepted = 0;
    std::uint32_t attemptBudget = txnCount * 4u;

    while (accepted < txnCount && attemptBudget > 0) {
      --attemptBudget;

      const std::int32_t offsetSec =
          math::timing::sampleOffset(rng, spender.timing);

      actors::Event event{};
      event.spender = &spender;
      event.ts = rates_.timestampAtOffset(offsetSec);
      event.exploreP = exploreP;

      auto maybeTxn = payments_.tryEmit(rng, event);
      if (!maybeTxn.has_value()) {
        continue;
      }

      outTxns.push_back(std::move(*maybeTxn));
      ++accepted;
    }

    rates_.recordAccepted(accepted);
  }
}

} // namespace PhantomLedger::spending::simulator
