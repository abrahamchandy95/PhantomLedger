#include "phantomledger/transfers/legit/routines/family/support.hpp"

#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::support {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;

namespace {

inline constexpr std::int64_t kPostingDayMaxExcl = 6;

/// Posting hour window [7am, 9pm).
inline constexpr std::int64_t kPostingHourMin = 7;
inline constexpr std::int64_t kPostingHourMaxExcl = 21;

/// Minimum credible support amount. Below this we drop.
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
             entity::Key retireeAcct, const Runtime &rt, random::Rng &rng) {
  const auto picked =
      fhelp::weightedPickPerson(supporters, rt.amountMultipliers, rng);
  if (!entity::valid(picked)) {
    return std::nullopt;
  }

  const auto payerAcct = fhelp::resolveFamilyAccount(
      picked, *rt.accounts, *rt.ownership, rt.routing.externalP);
  if (!payerAcct.has_value() || *payerAcct == retireeAcct) {
    return std::nullopt;
  }

  return ResolvedPayer{.person = picked, .account = *payerAcct};
}

[[nodiscard]] bool
emitMonthlySupport(::PhantomLedger::time::TimePoint monthStart,
                   std::span<const entity::PersonId> supporters,
                   entity::Key retireeAcct, std::int64_t windowEndEpochSec,
                   const SupportFlow &cfg, const Runtime &rt, random::Rng &rng,
                   std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.supportP)) {
    return false;
  }

  const auto payer = resolvePayer(supporters, retireeAcct, rt, rng);
  if (!payer.has_value()) {
    return false;
  }

  const auto ts = pickPostingTimestamp(monthStart, rng);
  if (ts >= windowEndEpochSec) {
    return false;
  }

  const auto base = fhelp::pareto(rng, cfg.paretoXm, cfg.paretoAlpha);
  const auto multiplier =
      fhelp::capacityFor(payer->person, rt.amountMultipliers);
  const auto amt = fhelp::sanitizeAmount(base * multiplier, kAmountFloor);
  if (amt == 0.0) {
    return false;
  }

  out.push_back(rt.txf->make(transactions::Draft{
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

std::vector<transactions::Transaction> generate(const Runtime &rt,
                                                const SupportFlow &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || rt.graph == nullptr || rt.accounts == nullptr ||
      rt.ownership == nullptr || rt.txf == nullptr ||
      rt.rngFactory == nullptr) {
    return out;
  }

  const auto personCount = rt.graph->personCount();
  if (personCount == 0 || rt.monthStarts.empty()) {
    return out;
  }

  // Count retirees-with-supporters for capacity estimate.
  std::size_t supportedRetireeCount = 0;
  for (entity::PersonId p = 1; p <= personCount; ++p) {
    if (pred::isRetired(rt.personas[p - 1]) &&
        !rt.graph->supportingChildrenOf[p - 1].empty()) {
      ++supportedRetireeCount;
    }
  }

  out.reserve(estimateCapacity(supportedRetireeCount, rt.monthStarts.size(),
                               cfg.supportP));

  auto rng = rt.rngFactory->rng({"family", "support"});

  const auto windowEndEpochSec =
      ::PhantomLedger::time::toEpochSeconds(rt.window.endExcl());

  for (entity::PersonId retiree = 1; retiree <= personCount; ++retiree) {
    if (!pred::isRetired(rt.personas[retiree - 1])) {
      continue;
    }

    const auto &supporters = rt.graph->supportingChildrenOf[retiree - 1];
    if (supporters.empty()) {
      continue;
    }

    const auto retireeAcct = fhelp::resolveFamilyAccount(
        retiree, *rt.accounts, *rt.ownership, /*externalP=*/0.0);
    if (!retireeAcct.has_value()) {
      continue;
    }

    const std::span<const entity::PersonId> supportersView{supporters};
    for (const auto monthStart : rt.monthStarts) {
      (void)emitMonthlySupport(monthStart, supportersView, *retireeAcct,
                               windowEndEpochSec, cfg, rt, rng, out);
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::support
