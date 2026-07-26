#include "phantomledger/activity/spending/simulator/loop.hpp"

#include "phantomledger/activity/spending/actors/counts.hpp"
#include "phantomledger/activity/spending/actors/event.hpp"
#include "phantomledger/activity/spending/actors/explore.hpp"
#include "phantomledger/activity/spending/diagnostics.hpp"
#include "phantomledger/activity/spending/liquidity/factor.hpp"
#include "phantomledger/activity/spending/liquidity/multiplier.hpp"
#include "phantomledger/activity/spending/liquidity/snapshot.hpp"
#include "phantomledger/activity/spending/routing/channel.hpp"
#include "phantomledger/activity/spending/routing/router.hpp"
#include "phantomledger/math/timing.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace PhantomLedger::activity::spending::simulator {

namespace {

using Ledger = ::PhantomLedger::clearing::Ledger;

constexpr std::uint32_t kAttemptMultiplier = 4;
constexpr std::uint32_t kMaxConsecutiveFailures = 3;

[[nodiscard]] inline std::uint16_t
personaBucketOf(const actors::Spender &spender) noexcept {
  return static_cast<std::uint16_t>(spender.personaType);
}

[[nodiscard]] inline double readLiquidity(const Gate &gate, Ledger::Index idx,
                                          double fallback) noexcept {
  if (!gate.attached() || idx == Ledger::invalid) {
    return fallback;
  }
  return gate.liquidity(idx);
}

[[nodiscard]] inline double readCash(const Gate &gate, Ledger::Index idx,
                                     double fallback) noexcept {
  if (!gate.attached() || idx == Ledger::invalid) {
    return fallback;
  }
  return gate.availableCash(idx);
}

} // namespace

SpenderEmissionLoop::RateSampler::RateSampler(const PreparedRun::Budget &budget,
                                              RunState &state,
                                              const actors::DayFrame &frame,
                                              Rules rules) noexcept
    : budget_(budget), state_(state), frame_(frame), rules_(rules),
      // H1 step 2b (class P) + H4: one CPI level lookup and one REAL
      // consumption level lookup per day frame; both engines share
      // this code, so oracle parity is automatic.
      dayPriceScale_(synth::econ::priceScale(
          time::toCalendarDate(frame.day.start).year)),
      dayRealLevel_(synth::econ::realPceLevel(
          time::toCalendarDate(frame.day.start).year)) {}

SpenderEmissionLoop::RateSampler &
SpenderEmissionLoop::RateSampler::dailyMultipliers(
    std::span<const double> value) noexcept {
  dailyMultipliers_ = value;
  return *this;
}

SpenderEmissionLoop::RateSampler &
SpenderEmissionLoop::RateSampler::ledgerView(Gate value) noexcept {
  ledgerView_ = value;
  return *this;
}

double SpenderEmissionLoop::RateSampler::availableToSpendFor(
    const spenders::PreparedSpender &prepared) {
  return readLiquidity(ledgerView_, prepared.spender.depositAccountIdx,
                       prepared.initialCash);
}

double SpenderEmissionLoop::RateSampler::availableCashFor(
    const spenders::PreparedSpender &prepared) {
  return readCash(ledgerView_, prepared.spender.depositAccountIdx,
                  prepared.initialCash);
}

double SpenderEmissionLoop::RateSampler::cardLiquidityFor(
    const spenders::PreparedSpender &prepared) {
  if (!prepared.spender.hasCard) {
    return 0.0;
  }
  return readLiquidity(ledgerView_, prepared.spender.cardIdx, 0.0);
}

double SpenderEmissionLoop::RateSampler::liquidityMultiplierFor(
    const spenders::PreparedSpender &prepared) {
  const auto personIndex = prepared.spender.personIndex;

  const liquidity::Snapshot snapshot{
      .daysSincePayday = state_.daysSincePayday(personIndex),
      .paycheckSensitivity = prepared.paycheckSensitivity,
      .availableToSpend = availableToSpendFor(prepared),
      .baselineCash = prepared.baselineCash,
      .fixedMonthlyBurden = prepared.fixedBurden,
      .priceScale = dayPriceScale_,
  };

  const auto mult = liquidity::multiplier(rules_.liquidity, snapshot);
  diagnostics::Stats::instance().recordLiquidityMultiplier(mult);

  lastLiquidityMult_ = mult;
  lastAvailableToSpend_ = snapshot.availableToSpend;
  return mult;
}

double SpenderEmissionLoop::RateSampler::combinedMultiplierFor(
    std::uint32_t personIndex) const {
  // H4 (authority U-9): the real per-capita consumption level
  // modulates the COUNT axis here — the budget keeps its meaning as a
  // CALIBRATION-LEVEL target, so realized session volume is
  // target x realPceLevel(year): a 2019 frame multiplies by exactly
  // 1.0, a 1991 frame runs at ~0.67x. Amounts are untouched (the
  // count-only channel law).
  return dailyMultipliers_[personIndex] * frame_.seasonalMult *
         dayRealLevel_;
}

double
SpenderEmissionLoop::RateSampler::latentBaseRateFor(const actors::Spender &,
                                                    DailyMultipliers) const {
  if (budget_.totalPersonDays == 0) {
    return 0.0;
  }
  return budget_.targetTotalTxns / static_cast<double>(budget_.totalPersonDays);
}

std::uint32_t SpenderEmissionLoop::RateSampler::transactionCountFor(
    random::Rng &rng, const actors::Spender &spender, double latentBaseRate,
    DailyMultipliers mults) const {
  const auto cnt =
      actors::sampleTransactionCount(rng, spender,
                                     actors::RatePieces{
                                         .baseRate = latentBaseRate,
                                         .weekdayMult = frame_.weekdayMult,
                                         .dynamicsMultiplier = mults.combined,
                                         .liquidityMultiplier = mults.liquidity,
                                         .dayShock = frame_.day.shock,
                                     },
                                     budget_.personLimit, rules_.rates);

  diagnostics::Stats::instance().recordCountSampled(cnt);
  return cnt;
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

double SpenderEmissionLoop::RateSampler::lastLiquidityMult() const noexcept {
  return lastLiquidityMult_;
}

double SpenderEmissionLoop::RateSampler::lastAvailableToSpend() const noexcept {
  return lastAvailableToSpend_;
}

double SpenderEmissionLoop::RateSampler::dayPriceScale() const noexcept {
  return dayPriceScale_;
}

std::uint32_t
SpenderEmissionLoop::RateSampler::frameDayIndex() const noexcept {
  return frame_.day.dayIndex;
}

SpenderEmissionLoop::PaymentEmitter::PaymentEmitter(
    const market::Market &market, const PreparedRun::Routing &routing,
    const transactions::Factory &factory, Gate gate) noexcept
    : market_(market), routing_(routing), factory_(factory),
      resolved_(routing.resolvedAccounts()), ledgerView_(gate) {}

void SpenderEmissionLoop::PaymentEmitter::bindRateSampler(
    const RateSampler *sampler) noexcept {
  rateSampler_ = sampler;
}

std::optional<SpenderEmissionLoop::PaymentEmitter::Emitted>
SpenderEmissionLoop::PaymentEmitter::tryEmit(random::Rng &rng,
                                             const actors::Event &event) {
  auto &stats = diagnostics::Stats::instance();

  const auto slot = routing::pickSlot(routing_.channelCdf, rng.nextDouble());
  const auto personaBucket = personaBucketOf(*event.spender);
  stats.recordAttempt(slot, personaBucket);

  const double liqMult =
      rateSampler_ != nullptr ? rateSampler_->lastLiquidityMult() : 0.0;
  const double avail =
      rateSampler_ != nullptr ? rateSampler_->lastAvailableToSpend() : 0.0;

  routing::PaymentRouter router{rng, market_, routing_.paymentRules, resolved_};
  auto maybeResult = router.route(slot, event);

  if (!maybeResult.has_value()) {
    stats.recordRouteNullopt(0u, event.spender->personIndex, personaBucket,
                             slot, liqMult, avail);
    return std::nullopt;
  }

  auto txn = factory_.rebound(rng).make(maybeResult->draft);

  const auto decision =
      ledgerView_.decide(maybeResult->srcIdx, maybeResult->dstIdx,
                         maybeResult->draft.amount, maybeResult->draft.channel);

  if (decision.rejected()) {
    stats.recordTransferRejected(0u, event.spender->personIndex, personaBucket,
                                 slot, decision.reason(), liqMult, avail);
    return std::nullopt;
  }

  stats.recordEmitted(slot, personaBucket);
  return Emitted{
      .txn = std::move(txn),
      .posting = {.srcIdx = maybeResult->srcIdx,
                  .dstIdx = maybeResult->dstIdx,
                  .amount = maybeResult->draft.amount,
                  .channel = maybeResult->draft.channel,
                  .timestamp = 0},
  };
}

SpenderEmissionLoop::SpenderEmissionLoop(
    const PreparedRun::Population &population, RateSampler &rates,
    PaymentEmitter &payments) noexcept
    : population_(population), rates_(rates), payments_(payments) {
  payments_.bindRateSampler(&rates_);
}

void SpenderEmissionLoop::run(
    std::size_t begin, std::size_t end, std::span<random::Rng> spenderRngs,
    std::vector<transactions::Transaction> &outTxns,
    std::vector<clearing::Ledger::Posting> &outPostings) {
  const auto &spenders = population_.spenders;

  for (std::size_t i = begin; i < end; ++i) {
    const auto &prepared = spenders[i];
    const auto &spender = prepared.spender;
    const auto personIndex = spender.personIndex;
    auto &rng = spenderRngs[i];

    // H3: the dead emit no person-days. The skip consumes the
    // person-day (budget bookkeeping identical to a zero-count day)
    // and draws NOTHING on the spender's per-person rng —
    // deterministic and thread-partition-safe.
    if (spender.deathDay != actors::Spender::kNoDeathDay &&
        rates_.frameDayIndex() >= spender.deathDay) {
      rates_.consumeOnePersonDay();
      continue;
    }

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
    const double availableCash = rates_.availableCashFor(prepared);
    const double cardAvailable = rates_.cardLiquidityFor(prepared);
    const double amountFactor = liquidity::amountFactor(liquidityMult);

    // H2 step 2c: the retirement consumption step — a level factor from
    // the claiming day onward. Pure derived data (no draws), shared by
    // both engines through this loop.
    const double consumptionScale =
        (spender.retireDay != actors::Spender::kNoRetireDay &&
         rates_.frameDayIndex() >= spender.retireDay)
            ? actors::kRetiredSpendScale
            : 1.0;

    std::uint32_t accepted = 0;
    std::uint32_t consecutiveFailures = 0;
    std::uint32_t attemptBudget = txnCount * kAttemptMultiplier;

    while (accepted < txnCount && attemptBudget > 0) {
      --attemptBudget;

      const auto offsetSec = math::timing::sampleOffset(rng, spender.timing);
      const actors::Event event{
          .spender = &spender,
          .ts = rates_.timestampAtOffset(offsetSec),
          .exploreP = exploreP,
          .availableCash = availableCash,
          .cardAvailable = cardAvailable,
          .amountFactor = amountFactor,
          .priceScale = rates_.dayPriceScale(),
          .consumptionScale = consumptionScale,
      };

      auto maybeEmitted = payments_.tryEmit(rng, event);
      if (!maybeEmitted.has_value()) {
        if (++consecutiveFailures >= kMaxConsecutiveFailures) {
          break;
        }
        continue;
      }

      consecutiveFailures = 0;
      outTxns.push_back(std::move(maybeEmitted->txn));
      outPostings.push_back(maybeEmitted->posting);
      ++accepted;
    }

    rates_.recordAccepted(accepted);
  }
}

} // namespace PhantomLedger::activity::spending::simulator
