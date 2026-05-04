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
    const actors::Spender &spender, double combinedMult,
    double liquidityMult) const {
  const std::uint64_t remainingPersonDays =
      std::max<std::uint64_t>(std::uint64_t{1}, state_.remainingPersonDays());

  const double targetRealizedPerDay =
      state_.remainingTargetTxns() / static_cast<double>(remainingPersonDays);

  return spenders::baseRateForTarget(spender, frame_.day.shock,
                                     frame_.weekdayMult, targetRealizedPerDay,
                                     combinedMult, liquidityMult);
}

std::uint32_t SpenderEmissionLoop::RateSampler::transactionCountFor(
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
    const routing::ResolvedAccounts &resolved,
    ParallelLedgerView ledgerView) noexcept
    : market_(market), routing_(routing), resolved_(resolved),
      ledgerView_(ledgerView) {}

bool SpenderEmissionLoop::PaymentEmitter::accept(
    const routing::EmissionResult &result) {
  return ledgerView_
      .transfer(result.srcIdx, result.dstIdx, result.transaction.amount,
                result.transaction.session.channel)
      .accepted();
}

std::optional<transactions::Transaction>
SpenderEmissionLoop::PaymentEmitter::tryEmit(random::Rng &rng,
                                             const actors::Event &event) {
  const routing::Slot slot =
      routing::pickSlot(routing_.channelCdf, rng.nextDouble());

  auto maybeResult = routing::routeTxn(rng, market_, routing_.paymentRules,
                                       resolved_, slot, event);

  if (!maybeResult.has_value()) {
    return std::nullopt;
  }

  if (!accept(*maybeResult)) {
    return std::nullopt;
  }

  return std::move(maybeResult->transaction);
}

SpenderEmissionLoop::SpenderEmissionLoop(
    const PreparedRun::Population &population, RateSampler &rates,
    PaymentEmitter &payments) noexcept
    : population_(population), rates_(rates), payments_(payments) {}

void SpenderEmissionLoop::run(std::size_t begin, std::size_t end,
                              random::Rng &rng,
                              const transactions::Factory &factory,
                              std::vector<transactions::Transaction> &outTxns) {
  const auto &spenders = population_.spenders;

  for (std::size_t i = begin; i < end; ++i) {
    const auto &prepared = spenders[i];
    const auto &spender = prepared.spender;
    const auto personIndex = spender.personIndex;

    const double liquidityMult = rates_.liquidityMultiplierFor(prepared);
    const double combinedMult = rates_.combinedMultiplierFor(personIndex);

    const double latentBaseRate =
        rates_.latentBaseRateFor(spender, combinedMult, liquidityMult);

    const auto txnCount = rates_.transactionCountFor(
        rng, spender, latentBaseRate, combinedMult, liquidityMult);

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
      event.factory = &factory;
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
