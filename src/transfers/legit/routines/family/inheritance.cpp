#include "phantomledger/transfers/legit/routines/family/inheritance.hpp"

#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::inheritance {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;
namespace dist = ::PhantomLedger::probability::distributions;

namespace {

inline constexpr double kTotalFloor = 1000.0;
inline constexpr double kPerHeirAmountFloor = 1.0;

inline constexpr std::int64_t kPostingHourMin = 10;
inline constexpr std::int64_t kPostingHourMaxExcl = 16;

[[nodiscard]] std::span<const entity::PersonId>
resolveHeirs(entity::PersonId retiree, const TransferRun &run) {
  const auto &direct = run.kinship().childrenOf(retiree);
  if (!direct.empty()) {
    return std::span<const entity::PersonId>{direct};
  }
  return std::span<const entity::PersonId>{
      run.kinship().supportingChildrenOf(retiree)};
}

[[nodiscard]] std::int64_t
pickEventTimestamp(::PhantomLedger::time::TimePoint windowStart, int windowDays,
                   random::Rng &rng) {
  const auto day = rng.uniformInt(0, std::max<std::int64_t>(1, windowDays));
  const auto hour = rng.uniformInt(kPostingHourMin, kPostingHourMaxExcl);
  const auto minute = rng.uniformInt(0, 60);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(windowStart, static_cast<int>(day)));
  return base + hour * 3600 + minute * 60;
}

void emitToHeir(entity::PersonId heir, entity::Key retireeAcct,
                double perHeirAmount, std::int64_t timestamp,
                const TransferRun &run,
                std::vector<transactions::Transaction> &out) {
  const auto heirAcct = run.accounts().routedMemberAccount(heir);
  if (!heirAcct.has_value() || *heirAcct == retireeAcct) {
    return;
  }

  const auto amt = fhelp::sanitizeAmount(perHeirAmount, kPerHeirAmountFloor);
  if (amt == 0.0) {
    return;
  }

  out.push_back(run.emission().make(transactions::Draft{
      .source = retireeAcct,
      .destination = *heirAcct,
      .amount = amt,
      .timestamp = timestamp,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::inheritance),
  }));
}

void processRetiree(entity::PersonId retiree, std::int64_t windowEndEpochSec,
                    const InheritanceEvent &cfg, const TransferRun &run,
                    random::Rng &rng,
                    std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.eventP)) {
    return;
  }

  const auto heirs = resolveHeirs(retiree, run);
  if (heirs.empty()) {
    return;
  }

  const auto retireeAcct = run.accounts().localMemberAccount(retiree);
  if (!retireeAcct.has_value()) {
    return;
  }

  const auto rawTotal = dist::lognormalByMedian(rng, cfg.median, cfg.sigma);
  const auto total = std::max(kTotalFloor, rawTotal);
  const auto perHeir =
      fhelp::roundCents(total / static_cast<double>(heirs.size()));

  const auto ts =
      pickEventTimestamp(run.posting().start(), run.posting().days(), rng);
  if (ts >= windowEndEpochSec) {
    return;
  }

  for (const auto heir : heirs) {
    emitToHeir(heir, *retireeAcct, perHeir, ts, run, out);
  }
}

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const InheritanceEvent &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0) {
    return out;
  }

  auto rng = run.emission().rng({"family", "inheritance"});

  out.reserve(16);

  const auto windowEndEpochSec = run.posting().endEpochSec();

  for (entity::PersonId retiree = 1; retiree <= personCount; ++retiree) {
    if (!pred::isRetired(run.kinship().persona(retiree))) {
      continue;
    }
    processRetiree(retiree, windowEndEpochSec, cfg, run, rng, out);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::inheritance
