#include "phantomledger/transfers/legit/routines/family/spouse.hpp"

#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <cstdint>

namespace PhantomLedger::transfers::legit::routines::family::spouse {

namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;
namespace dist = ::PhantomLedger::probability::distributions;

namespace {

inline constexpr std::int64_t kPostingDayMaxExcl = 28;
inline constexpr std::int64_t kPostingHourMin = 7;
inline constexpr std::int64_t kPostingHourMaxExcl = 22;
inline constexpr double kAmountFloor = 5.0;

struct CoupleState {
  entity::Key higherAcct{};
  entity::Key lowerAcct{};
};

[[nodiscard]] std::optional<CoupleState>
resolveCouple(entity::PersonId a, entity::PersonId b, const CoupleFlow &cfg,
              const Runtime &rt, random::Rng &rng) {
  if (!rng.coin(cfg.separateAccountsP)) {
    return std::nullopt;
  }

  const auto acctA = fhelp::resolveFamilyAccount(a, *rt.accounts, *rt.ownership,
                                                 rt.routing.externalP);
  const auto acctB = fhelp::resolveFamilyAccount(b, *rt.accounts, *rt.ownership,
                                                 rt.routing.externalP);
  if (!acctA.has_value() || !acctB.has_value() || *acctA == *acctB) {
    return std::nullopt;
  }
  if (encoding::isExternal(*acctA) && encoding::isExternal(*acctB)) {
    return std::nullopt;
  }

  // Capacity multiplier as breadwinner proxy. Equal weights →
  // a is the higher, deterministic.
  const auto powerA = fhelp::capacityFor(a, rt.amountMultipliers);
  const auto powerB = fhelp::capacityFor(b, rt.amountMultipliers);

  if (powerA >= powerB) {
    return CoupleState{.higherAcct = *acctA, .lowerAcct = *acctB};
  }
  return CoupleState{.higherAcct = *acctB, .lowerAcct = *acctA};
}

[[nodiscard]] bool
emitOneTransfer(::PhantomLedger::time::TimePoint monthStart,
                const CoupleState &couple, std::int64_t windowEndEpochSec,
                const CoupleFlow &cfg, const Runtime &rt, random::Rng &rng,
                std::vector<transactions::Transaction> &out) {
  const bool fromBreadwinner = rng.coin(cfg.breadwinnerFlowP);
  const auto src = fromBreadwinner ? couple.higherAcct : couple.lowerAcct;
  const auto dst = fromBreadwinner ? couple.lowerAcct : couple.higherAcct;

  const auto day = rng.uniformInt(0, kPostingDayMaxExcl);
  const auto hour = rng.uniformInt(kPostingHourMin, kPostingHourMaxExcl);
  const auto minute = rng.uniformInt(0, 60);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(monthStart, static_cast<int>(day)));
  const auto ts = base + hour * 3600 + minute * 60;
  if (ts >= windowEndEpochSec) {
    return false;
  }

  const auto raw =
      dist::lognormalByMedian(rng, cfg.transferMedian, cfg.transferSigma);
  const auto amt = fhelp::sanitizeAmount(raw, kAmountFloor);
  if (amt == 0.0) {
    return false;
  }

  out.push_back(rt.txf->make(transactions::Draft{
      .source = src,
      .destination = dst,
      .amount = amt,
      .timestamp = ts,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::spouseTransfer),
  }));
  return true;
}

void processCoupleMonth(::PhantomLedger::time::TimePoint monthStart,
                        const CoupleState &couple,
                        std::int64_t windowEndEpochSec, const CoupleFlow &cfg,
                        const Runtime &rt, random::Rng &rng,
                        std::vector<transactions::Transaction> &out) {
  const auto count = static_cast<int>(rng.uniformInt(
      cfg.txnsPerMonthMin, static_cast<std::int64_t>(cfg.txnsPerMonthMax) + 1));

  for (int i = 0; i < count; ++i) {
    if (!emitOneTransfer(monthStart, couple, windowEndEpochSec, cfg, rt, rng,
                         out)) {
    }
  }
}

} // namespace

std::vector<transactions::Transaction> generate(const Runtime &rt,
                                                const CoupleFlow &cfg) {
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

  auto rng = rt.rngFactory->rng({"family", "spouse"});

  const auto avgPerMonth = (cfg.txnsPerMonthMin + cfg.txnsPerMonthMax + 1) / 2;
  out.reserve(static_cast<std::size_t>(personCount / 4U) *
              rt.monthStarts.size() *
              static_cast<std::size_t>(std::max(1, avgPerMonth)));

  const auto windowEndEpochSec =
      ::PhantomLedger::time::toEpochSeconds(rt.window.endExcl());

  for (entity::PersonId p = 1; p <= personCount; ++p) {
    const auto spouse = rt.graph->spouseOf[p - 1];
    if (!entity::valid(spouse) || spouse <= p) {
      continue;
    }

    const auto couple = resolveCouple(p, spouse, cfg, rt, rng);
    if (!couple.has_value()) {
      continue;
    }

    for (const auto monthStart : rt.monthStarts) {
      processCoupleMonth(monthStart, *couple, windowEndEpochSec, cfg, rt, rng,
                         out);
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::spouse
