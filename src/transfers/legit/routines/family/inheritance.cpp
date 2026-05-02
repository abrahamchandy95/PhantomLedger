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

/// Posting time — daytime hours [10am, 4pm).
inline constexpr std::int64_t kPostingHourMin = 10;
inline constexpr std::int64_t kPostingHourMaxExcl = 16;

[[nodiscard]] std::span<const entity::PersonId>
resolveHeirs(entity::PersonId retiree, const Runtime &rt) {
  const auto &direct = rt.graph->childrenOf[retiree - 1];
  if (!direct.empty()) {
    return std::span<const entity::PersonId>{direct};
  }
  return std::span<const entity::PersonId>{
      rt.graph->supportingChildrenOf[retiree - 1]};
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
                double perHeirAmount, std::int64_t timestamp, const Runtime &rt,
                std::vector<transactions::Transaction> &out) {
  const auto heirAcct = fhelp::resolveFamilyAccount(
      heir, *rt.accounts, *rt.ownership, rt.routing.externalP);
  if (!heirAcct.has_value() || *heirAcct == retireeAcct) {
    return;
  }

  const auto amt = fhelp::sanitizeAmount(perHeirAmount, kPerHeirAmountFloor);
  if (amt == 0.0) {
    return;
  }

  out.push_back(rt.txf->make(transactions::Draft{
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
                    const InheritanceEvent &cfg, const Runtime &rt,
                    random::Rng &rng,
                    std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.eventP)) {
    return;
  }

  const auto heirs = resolveHeirs(retiree, rt);
  if (heirs.empty()) {
    return;
  }

  const auto retireeAcct = fhelp::resolveFamilyAccount(
      retiree, *rt.accounts, *rt.ownership, /*externalP=*/0.0);
  if (!retireeAcct.has_value()) {
    return;
  }

  const auto rawTotal = dist::lognormalByMedian(rng, cfg.median, cfg.sigma);
  const auto total = std::max(kTotalFloor, rawTotal);
  const auto perHeir =
      fhelp::roundCents(total / static_cast<double>(heirs.size()));

  const auto ts = pickEventTimestamp(rt.window.start, rt.window.days, rng);
  if (ts >= windowEndEpochSec) {
    return;
  }

  for (const auto heir : heirs) {
    emitToHeir(heir, *retireeAcct, perHeir, ts, rt, out);
  }
}

} // namespace

std::vector<transactions::Transaction> generate(const Runtime &rt,
                                                const InheritanceEvent &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || rt.graph == nullptr || rt.accounts == nullptr ||
      rt.ownership == nullptr || rt.txf == nullptr ||
      rt.rngFactory == nullptr) {
    return out;
  }

  const auto personCount = rt.graph->personCount();
  if (personCount == 0) {
    return out;
  }

  auto rng = rt.rngFactory->rng({"family", "inheritance"});

  out.reserve(16);

  const auto windowEndEpochSec =
      ::PhantomLedger::time::toEpochSeconds(rt.window.endExcl());

  for (entity::PersonId retiree = 1; retiree <= personCount; ++retiree) {
    if (!pred::isRetired(rt.personas[retiree - 1])) {
      continue;
    }
    processRetiree(retiree, windowEndEpochSec, cfg, rt, rng, out);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::inheritance
