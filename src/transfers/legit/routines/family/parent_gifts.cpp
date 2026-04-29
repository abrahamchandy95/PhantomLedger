#include "phantomledger/transfers/legit/routines/family/parent_gifts.hpp"

#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <array>
#include <cstdint>

namespace PhantomLedger::transfers::legit::routines::family::parent_gifts {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;

namespace {

inline constexpr std::int64_t kPostingDayMaxExcl = 20;

/// Posting hour [8am, 9pm).
inline constexpr std::int64_t kPostingHourMin = 8;
inline constexpr std::int64_t kPostingHourMaxExcl = 21;

/// Floor on per-gift amount.
inline constexpr double kAmountFloor = 10.0;

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

[[nodiscard]] std::size_t
collectGivingParents(std::span<const entity::PersonId> parents,
                     std::span<const ::PhantomLedger::personas::Type> personas,
                     std::array<entity::PersonId, 2> &out) {
  std::size_t count = 0;
  for (const auto p : parents) {
    if (!entity::valid(p)) {
      continue;
    }
    if (pred::canSupport(personas[p - 1])) {
      out[count++] = p;
    }
  }
  return count;
}

[[nodiscard]] bool
emitMonthlyGift(::PhantomLedger::time::TimePoint monthStart,
                std::span<const entity::PersonId> givingParents,
                entity::Key childAcct, std::int64_t windowEndEpochSec,
                const ::PhantomLedger::transfers::family::ParentGifts &cfg,
                const Runtime &rt, random::Rng &rng,
                std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.p)) {
    return false;
  }

  const auto parentIdx = static_cast<std::size_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(givingParents.size())));
  const auto payerId = givingParents[parentIdx];

  const auto payerAcct = fhelp::resolveFamilyAccount(
      payerId, *rt.accounts, *rt.ownership, rt.routing.externalP);
  if (!payerAcct.has_value() || *payerAcct == childAcct) {
    return false;
  }

  const auto ts = pickPostingTimestamp(monthStart, rng);
  if (ts >= windowEndEpochSec) {
    return false;
  }

  const auto base = fhelp::pareto(rng, cfg.paretoXm, cfg.paretoAlpha);
  const auto multiplier = fhelp::capacityFor(payerId, rt.amountMultipliers);
  const auto amt = fhelp::sanitizeAmount(base * multiplier, kAmountFloor);
  if (amt == 0.0) {
    return false;
  }

  out.push_back(rt.txf->make(transactions::Draft{
      .source = *payerAcct,
      .destination = childAcct,
      .amount = amt,
      .timestamp = ts,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::parentGift),
  }));
  return true;
}

} // namespace

std::vector<transactions::Transaction>
generate(const Runtime &rt,
         const ::PhantomLedger::transfers::family::ParentGifts &cfg) {
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

  auto rng = rt.rngFactory->rng({"family", "parent_gifts"});

  // Capacity estimate: rough — most adults won't qualify.
  out.reserve(static_cast<std::size_t>(
      static_cast<double>(personCount) *
      static_cast<double>(rt.monthStarts.size()) * cfg.p * 0.1));

  const auto windowEndEpochSec =
      ::PhantomLedger::time::toEpochSeconds(rt.window.endExcl());

  std::array<entity::PersonId, 2> givingParents{};

  for (entity::PersonId child = 1; child <= personCount; ++child) {
    if (!pred::isAdult(rt.personas[child - 1])) {
      continue;
    }

    const auto parentSlots = rt.graph->parentsOf[child - 1];
    if (!entity::valid(parentSlots[0]) && !entity::valid(parentSlots[1])) {
      continue;
    }

    const auto givingCount = collectGivingParents(
        std::span<const entity::PersonId>{parentSlots.data(),
                                          parentSlots.size()},
        rt.personas, givingParents);
    if (givingCount == 0) {
      continue;
    }

    const auto childAcct = fhelp::resolveFamilyAccount(
        child, *rt.accounts, *rt.ownership, /*externalP=*/0.0);
    if (!childAcct.has_value()) {
      continue;
    }

    const std::span<const entity::PersonId> givingView{givingParents.data(),
                                                       givingCount};
    for (const auto monthStart : rt.monthStarts) {
      (void)emitMonthlyGift(monthStart, givingView, *childAcct,
                            windowEndEpochSec, cfg, rt, rng, out);
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::parent_gifts
