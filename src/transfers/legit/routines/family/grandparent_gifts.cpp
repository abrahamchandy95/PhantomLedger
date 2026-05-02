#include "phantomledger/transfers/legit/routines/family/grandparent_gifts.hpp"

#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <cstdint>

namespace PhantomLedger::transfers::legit::routines::family::grandparent_gifts {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;
namespace dist = ::PhantomLedger::probability::distributions;

namespace {

inline constexpr std::int64_t kPostingDayMaxExcl = 20;
inline constexpr std::int64_t kPostingHourMin = 8;
inline constexpr std::int64_t kPostingHourMaxExcl = 20;
inline constexpr double kAmountFloor = 10.0;

[[nodiscard]] bool
emitMonthlyGift(::PhantomLedger::time::TimePoint monthStart, entity::Key gpAcct,
                entity::Key gcAcct, std::int64_t windowEndEpochSec,
                const GrandparentGiftFlow &cfg, const Runtime &rt,
                random::Rng &rng, std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.p)) {
    return false;
  }

  const auto day = rng.uniformInt(0, kPostingDayMaxExcl);
  const auto hour = rng.uniformInt(kPostingHourMin, kPostingHourMaxExcl);
  const auto minute = rng.uniformInt(0, 60);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(monthStart, static_cast<int>(day)));
  const auto ts = base + hour * 3600 + minute * 60;
  if (ts >= windowEndEpochSec) {
    return false;
  }

  const auto raw = dist::lognormalByMedian(rng, cfg.median, cfg.sigma);
  const auto amt = fhelp::sanitizeAmount(raw, kAmountFloor);
  if (amt == 0.0) {
    return false;
  }

  out.push_back(rt.txf->make(transactions::Draft{
      .source = gpAcct,
      .destination = gcAcct,
      .amount = amt,
      .timestamp = ts,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::grandparentGift),
  }));
  return true;
}

void processPair(entity::PersonId grandparent, entity::PersonId grandchild,
                 std::int64_t windowEndEpochSec, const GrandparentGiftFlow &cfg,
                 const Runtime &rt, random::Rng &rng,
                 std::vector<transactions::Transaction> &out) {
  const auto gpAcct = fhelp::resolveFamilyAccount(
      grandparent, *rt.accounts, *rt.ownership, /*externalP=*/0.0);
  const auto gcAcct = fhelp::resolveFamilyAccount(
      grandchild, *rt.accounts, *rt.ownership, /*externalP=*/0.0);
  if (!gpAcct.has_value() || !gcAcct.has_value() || *gpAcct == *gcAcct) {
    return;
  }

  for (const auto monthStart : rt.monthStarts) {
    (void)emitMonthlyGift(monthStart, *gpAcct, *gcAcct, windowEndEpochSec, cfg,
                          rt, rng, out);
  }
}

} // namespace

std::vector<transactions::Transaction>
generate(const Runtime &rt, const GrandparentGiftFlow &cfg) {
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

  auto rng = rt.rngFactory->rng({"family", "grandparent_gifts"});

  const auto windowEndEpochSec =
      ::PhantomLedger::time::toEpochSeconds(rt.window.endExcl());

  for (entity::PersonId gp = 1; gp <= personCount; ++gp) {
    if (!pred::isRetired(rt.personas[gp - 1])) {
      continue;
    }

    const auto &middleGen = rt.graph->childrenOf[gp - 1];
    for (const auto parent : middleGen) {
      if (!entity::valid(parent) || parent > personCount) {
        continue;
      }
      const auto &grandchildren = rt.graph->childrenOf[parent - 1];
      for (const auto gc : grandchildren) {
        processPair(gp, gc, windowEndEpochSec, cfg, rt, rng, out);
      }
    }
  }

  return out;
}

} // namespace
  // PhantomLedger::transfers::legit::routines::family::grandparent_gifts
