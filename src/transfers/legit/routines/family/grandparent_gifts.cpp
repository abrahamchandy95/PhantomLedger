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
                const GrandparentGiftFlow &cfg, const TransferRun &run,
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

  out.push_back(run.emission().make(transactions::Draft{
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
                 const TransferRun &run, random::Rng &rng,
                 std::vector<transactions::Transaction> &out) {
  const auto gpAcct = run.accounts().localMemberAccount(grandparent);
  const auto gcAcct = run.accounts().localMemberAccount(grandchild);
  if (!gpAcct.has_value() || !gcAcct.has_value() || *gpAcct == *gcAcct) {
    return;
  }

  for (const auto monthStart : run.posting().monthStarts()) {
    (void)emitMonthlyGift(monthStart, *gpAcct, *gcAcct, windowEndEpochSec, cfg,
                          run, rng, out);
  }
}

} // namespace

std::vector<transactions::Transaction>
generate(const TransferRun &run, const GrandparentGiftFlow &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0 || run.posting().monthStarts().empty()) {
    return out;
  }

  auto rng = run.emission().rng({"family", "grandparent_gifts"});

  const auto windowEndEpochSec = run.posting().endEpochSec();

  for (entity::PersonId gp = 1; gp <= personCount; ++gp) {
    if (!pred::isRetired(run.kinship().persona(gp))) {
      continue;
    }

    const auto &middleGen = run.kinship().childrenOf(gp);
    for (const auto parent : middleGen) {
      if (!entity::valid(parent) || parent > personCount) {
        continue;
      }
      const auto &grandchildren = run.kinship().childrenOf(parent);
      for (const auto gc : grandchildren) {
        processPair(gp, gc, windowEndEpochSec, cfg, run, rng, out);
      }
    }
  }

  return out;
}

} // namespace
  // PhantomLedger::transfers::legit::routines::family::grandparent_gifts
