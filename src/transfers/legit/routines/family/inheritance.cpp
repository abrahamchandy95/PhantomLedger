#include "phantomledger/transfers/legit/routines/family/inheritance.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
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

class InheritanceEmitter {
public:
  InheritanceEmitter(const TransferRun &run, const InheritanceEvent &cfg,
                     random::Rng &rng,
                     std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  InheritanceEmitter(const InheritanceEmitter &) = delete;
  InheritanceEmitter &operator=(const InheritanceEmitter &) = delete;

  /// Process one retiree. Drops out early on the eventP gate, on
  /// missing heirs, or on a missing retiree account.
  void processRetiree(entity::PersonId retiree) {
    if (!rng_.coin(cfg_.eventP)) {
      return;
    }

    const auto heirs = resolveHeirs(retiree, run_);
    if (heirs.empty()) {
      return;
    }

    const auto retireeAcct = run_.accounts().localMemberAccount(retiree);
    if (!retireeAcct.has_value()) {
      return;
    }

    const auto perHeir = sampleEstateShare(heirs.size());
    const auto ts = pickEventTimestamp();
    if (ts >= windowEndEpochSec_) {
      return;
    }

    for (const auto heir : heirs) {
      emitToHeir(heir, *retireeAcct, perHeir, ts);
    }
  }

private:
  [[nodiscard]] double sampleEstateShare(std::size_t heirCount) {
    const auto rawTotal =
        dist::lognormalByMedian(rng_, cfg_.median, cfg_.sigma);
    const auto total = std::max(kTotalFloor, rawTotal);
    return fhelp::roundCents(total / static_cast<double>(heirCount));
  }

  [[nodiscard]] std::int64_t pickEventTimestamp() {
    const auto windowDays = run_.posting().days();
    const auto day = rng_.uniformInt(0, std::max<std::int64_t>(1, windowDays));
    const auto hour = rng_.uniformInt(kPostingHourMin, kPostingHourMaxExcl);
    const auto minute = rng_.uniformInt(0, 60);

    const auto base =
        ::PhantomLedger::time::toEpochSeconds(::PhantomLedger::time::addDays(
            run_.posting().start(), static_cast<int>(day)));
    return base + hour * 3600 + minute * 60;
  }

  void emitToHeir(entity::PersonId heir, entity::Key retireeAcct,
                  double perHeirAmount, std::int64_t timestamp) {
    const auto heirAcct = run_.accounts().routedMemberAccount(heir);
    if (!heirAcct.has_value() || *heirAcct == retireeAcct) {
      return;
    }

    const auto amount =
        fhelp::sanitizeAmount(perHeirAmount, kPerHeirAmountFloor);
    if (amount == 0.0) {
      return;
    }

    out_.push_back(run_.emission().make(transactions::Draft{
        .source = retireeAcct,
        .destination = *heirAcct,
        .amount = amount,
        .timestamp = timestamp,
        .isFraud = 0,
        .ringId = -1,
        .channel = channels::tag(channels::Family::inheritance),
    }));
  }

  const TransferRun &run_;
  const InheritanceEvent &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

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

  InheritanceEmitter emitter{run, cfg, rng, out};

  for (entity::PersonId retiree = 1; retiree <= personCount; ++retiree) {
    if (!pred::isRetired(run.kinship().persona(retiree))) {
      continue;
    }
    emitter.processRetiree(retiree);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::inheritance
