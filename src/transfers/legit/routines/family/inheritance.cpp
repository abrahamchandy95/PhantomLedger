#include "phantomledger/transfers/legit/routines/family/inheritance.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::inheritance {

namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;
namespace dist = ::PhantomLedger::probability::distributions;

namespace {

inline constexpr double kTotalFloor = 1000.0;
inline constexpr double kPerHeirAmountFloor = 1.0;

// Funeral: paid from the decedent's account a few days after the
// death; estates settle 30-90 days later (probate, declared).
inline constexpr std::int64_t kFuneralDayMin = 3;
inline constexpr std::int64_t kFuneralDayMaxExcl = 11;
inline constexpr std::int64_t kEstateDayMin = 30;
inline constexpr std::int64_t kEstateDayMaxExcl = 91;

inline constexpr std::int64_t kPostingHourMin = 10;
inline constexpr std::int64_t kPostingHourMaxExcl = 16;

// The external service-merchant hub (the same destination the spending
// router's external_unknown flow uses). A dedicated funeral-home
// counterparty + channel is a registered upgrade; the funeral rides
// the `bill` channel (a household service payment).
[[nodiscard]] entity::Key funeralPayee() noexcept {
  return entity::makeKey(entity::Role::merchant, entity::Bank::external, 1ULL);
}

[[nodiscard]] std::span<const entity::PersonId>
resolveHeirs(entity::PersonId decedent, const TransferRun &run) {
  const auto &direct = run.kinship().childrenOf(decedent);
  if (!direct.empty()) {
    return std::span<const entity::PersonId>{direct};
  }
  return std::span<const entity::PersonId>{
      run.kinship().supportingChildrenOf(decedent)};
}

class EstateEmitter {
public:
  EstateEmitter(const TransferRun &run, const InheritanceEvent &cfg,
                random::Rng &rng,
                std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowStartEpochSec_(::PhantomLedger::time::toEpochSeconds(
            run.posting().start())),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  EstateEmitter(const EstateEmitter &) = delete;
  EstateEmitter &operator=(const EstateEmitter &) = delete;

  void processPerson(entity::PersonId person) {
    const auto &tl = run_.kinship().timeline(person);
    const auto deathEpoch = ::PhantomLedger::time::toEpochSeconds(tl.death);
    if (deathEpoch < windowStartEpochSec_ ||
        deathEpoch >= windowEndEpochSec_) {
      return;
    }

    // The per-decedent draws, unconditional, in this fixed order
    // (contract): the emit decisions below cannot move a later
    // decedent's stream.
    const auto funeralRaw =
        dist::lognormalByMedian(rng_, cfg_.funeralMedian, cfg_.funeralSigma);
    const auto funeralTs = offsetTimestamp(deathEpoch, kFuneralDayMin,
                                           kFuneralDayMaxExcl);
    const auto estateRaw = dist::lognormalByMedian(rng_, cfg_.median,
                                                   cfg_.sigma);
    const auto estateTs =
        offsetTimestamp(deathEpoch, kEstateDayMin, kEstateDayMaxExcl);

    const auto decedentAcct = run_.accounts().localMemberAccount(person);
    if (!decedentAcct.has_value()) {
      return;
    }

    emitFuneral(*decedentAcct, funeralRaw, funeralTs);
    emitEstate(person, *decedentAcct, estateRaw, estateTs);
  }

private:
  [[nodiscard]] std::int64_t offsetTimestamp(std::int64_t deathEpoch,
                                             std::int64_t dayMin,
                                             std::int64_t dayMaxExcl) {
    const auto day = rng_.uniformInt(dayMin, dayMaxExcl);
    const auto hour = rng_.uniformInt(kPostingHourMin, kPostingHourMaxExcl);
    const auto minute = rng_.uniformInt(0, 60);
    return deathEpoch + day * 86'400 +
           ::PhantomLedger::time::secondsInDay(hour, minute);
  }

  void emitFuneral(entity::Key decedentAcct, double raw, std::int64_t ts) {
    if (ts >= windowEndEpochSec_) {
      return; // the window closed before the funeral posted (declared)
    }

    const auto amount = std::max(cfg_.funeralFloor, raw);

    // NFDA anchor is calibration dollars; realize at the death year's
    // price level (class P), like every family amount.
    out_.push_back(run_.emission().make(transactions::Draft{
        .source = decedentAcct,
        .destination = funeralPayee(),
        .amount = fhelp::nominalAt(primitives::utils::roundMoney(amount), ts),
        .timestamp = ts,
        .isFraud = 0,
        .ringId = -1,
        .channel = channels::tag(channels::Legit::bill),
    }));
  }

  void emitEstate(entity::PersonId decedent, entity::Key decedentAcct,
                  double rawTotal, std::int64_t ts) {
    if (ts >= windowEndEpochSec_) {
      return; // probate outlives the window (declared)
    }

    const auto heirs = resolveHeirs(decedent, run_);
    if (heirs.empty()) {
      return; // heirless estates are out of scope until part 3
    }

    const auto total = std::max(kTotalFloor, rawTotal);
    const auto perHeir = primitives::utils::roundMoney(
        total / static_cast<double>(heirs.size()));

    for (const auto heir : heirs) {
      const auto heirAcct = run_.accounts().routedMemberAccount(heir);
      if (!heirAcct.has_value() || *heirAcct == decedentAcct) {
        continue;
      }

      const auto amount =
          fhelp::sanitizeAmount(perHeir, kPerHeirAmountFloor);
      if (amount == 0.0) {
        continue;
      }

      // Estate shares scale at the settle date's CPI level (class P;
      // the SCF size re-derivation is the registered upgrade).
      out_.push_back(run_.emission().make(transactions::Draft{
          .source = decedentAcct,
          .destination = *heirAcct,
          .amount = fhelp::nominalAt(amount, ts),
          .timestamp = ts,
          .isFraud = 0,
          .ringId = -1,
          .channel = channels::tag(channels::Family::inheritance),
      }));
    }
  }

  const TransferRun &run_;
  const InheritanceEvent &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowStartEpochSec_;
  std::int64_t windowEndEpochSec_;
};

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const InheritanceEvent &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  // Death-caused estates need the timeline lane; hand-built views
  // without it emit nothing (the blueprint path always binds it).
  if (!run.kinship().hasTimelines()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0) {
    return out;
  }

  auto rng = run.emission().rng({"family", "inheritance"});

  out.reserve(16);

  EstateEmitter emitter{run, cfg, rng, out};

  for (entity::PersonId person = 1; person <= personCount; ++person) {
    emitter.processPerson(person);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::inheritance
