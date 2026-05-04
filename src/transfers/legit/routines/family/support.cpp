#include "phantomledger/transfers/legit/routines/family/support.hpp"

#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::support {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;

namespace {

inline constexpr std::int64_t kPostingDayMaxExcl = 6;
inline constexpr std::int64_t kPostingHourMin = 7;
inline constexpr std::int64_t kPostingHourMaxExcl = 21;
inline constexpr double kAmountFloor = 5.0;

[[nodiscard]] std::int64_t
pickPostingTimestamp(::PhantomLedger::time::TimePoint monthStart,
                     random::Rng &rng) {
  const auto day = rng.uniformInt(0, kPostingDayMaxExcl);
  const auto hour = rng.uniformInt(kPostingHourMin, kPostingHourMaxExcl);
  const auto minute = rng.uniformInt(0, 60);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(monthStart, static_cast<int>(day)));
  return base + hour * 3600 + minute * 60;
}

struct ResolvedPayer {
  entity::PersonId person = entity::invalidPerson;
  entity::Key account{};
};

[[nodiscard]] std::optional<ResolvedPayer>
resolvePayer(std::span<const entity::PersonId> supporters,
             entity::Key retireeAcct, const TransferRun &run,
             random::Rng &rng) {
  const auto picked = fhelp::weightedPickPerson(
      supporters, run.kinship().amountMultipliers(), rng);
  if (!entity::valid(picked)) {
    return std::nullopt;
  }

  const auto payerAcct = run.accounts().routedMemberAccount(picked);
  if (!payerAcct.has_value() || *payerAcct == retireeAcct) {
    return std::nullopt;
  }

  return ResolvedPayer{.person = picked, .account = *payerAcct};
}

[[nodiscard]] bool
emitMonthlySupport(::PhantomLedger::time::TimePoint monthStart,
                   std::span<const entity::PersonId> supporters,
                   entity::Key retireeAcct, std::int64_t windowEndEpochSec,
                   const SupportFlow &cfg, const TransferRun &run,
                   random::Rng &rng,
                   std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.supportP)) {
    return false;
  }

  const auto payer = resolvePayer(supporters, retireeAcct, run, rng);
  if (!payer.has_value()) {
    return false;
  }

  const auto ts = pickPostingTimestamp(monthStart, rng);
  if (ts >= windowEndEpochSec) {
    return false;
  }

  const auto base = fhelp::pareto(rng, cfg.paretoXm, cfg.paretoAlpha);
  const auto multiplier = run.kinship().amountMultiplier(payer->person);
  const auto amt = fhelp::sanitizeAmount(base * multiplier, kAmountFloor);
  if (amt == 0.0) {
    return false;
  }

  out.push_back(run.emission().make(transactions::Draft{
      .source = payer->account,
      .destination = retireeAcct,
      .amount = amt,
      .timestamp = ts,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::support),
  }));
  return true;
}

[[nodiscard]] std::size_t estimateCapacity(std::size_t retireeCount,
                                           std::size_t monthCount,
                                           double supportP) {
  if (retireeCount == 0 || monthCount == 0) {
    return 0;
  }
  const auto expected =
      static_cast<double>(retireeCount * monthCount) * supportP;
  return static_cast<std::size_t>(expected) + (retireeCount / 4U);
}

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const SupportFlow &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0 || run.posting().monthStarts().empty()) {
    return out;
  }

  std::size_t supportedRetireeCount = 0;
  for (entity::PersonId p = 1; p <= personCount; ++p) {
    if (pred::isRetired(run.kinship().persona(p)) &&
        !run.kinship().supportingChildrenOf(p).empty()) {
      ++supportedRetireeCount;
    }
  }

  out.reserve(estimateCapacity(
      supportedRetireeCount, run.posting().monthStarts().size(), cfg.supportP));

  auto rng = run.emission().rng({"family", "support"});

  const auto windowEndEpochSec = run.posting().endEpochSec();

  for (entity::PersonId retiree = 1; retiree <= personCount; ++retiree) {
    if (!pred::isRetired(run.kinship().persona(retiree))) {
      continue;
    }

    const auto &supporters = run.kinship().supportingChildrenOf(retiree);
    if (supporters.empty()) {
      continue;
    }

    const auto retireeAcct = run.accounts().localMemberAccount(retiree);
    if (!retireeAcct.has_value()) {
      continue;
    }

    const std::span<const entity::PersonId> supportersView{supporters};
    for (const auto monthStart : run.posting().monthStarts()) {
      (void)emitMonthlySupport(monthStart, supportersView, *retireeAcct,
                               windowEndEpochSec, cfg, run, rng, out);
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::support
