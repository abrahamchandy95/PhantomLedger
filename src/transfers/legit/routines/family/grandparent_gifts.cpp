#include "phantomledger/transfers/legit/routines/family/grandparent_gifts.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
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

/// Stateful, narrow-purpose emitter for grandparent-to-grandchild
/// gift transactions. Same pattern as `AllowanceEmitter`: dependency
/// views and sinks are members; only routine-shape arguments cross
/// method boundaries.
class GrandparentGiftEmitter {
public:
  GrandparentGiftEmitter(const TransferRun &run, const GrandparentGiftFlow &cfg,
                         random::Rng &rng,
                         std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  GrandparentGiftEmitter(const GrandparentGiftEmitter &) = delete;
  GrandparentGiftEmitter &operator=(const GrandparentGiftEmitter &) = delete;

  /// Walk all month-starts for one (grandparent, grandchild) pair,
  /// emitting zero or more gifts.
  void processPair(entity::PersonId grandparent, entity::PersonId grandchild) {
    const auto gpAcct = run_.accounts().localMemberAccount(grandparent);
    const auto gcAcct = run_.accounts().localMemberAccount(grandchild);
    if (!gpAcct.has_value() || !gcAcct.has_value() || *gpAcct == *gcAcct) {
      return;
    }

    for (const auto monthStart : run_.posting().monthStarts()) {
      (void)tryEmit(*gpAcct, *gcAcct, monthStart);
    }
  }

private:
  /// Try to emit a single monthly gift. Returns true on success.
  [[nodiscard]] bool tryEmit(entity::Key gpAcct, entity::Key gcAcct,
                             ::PhantomLedger::time::TimePoint monthStart) {
    if (!rng_.coin(cfg_.p)) {
      return false;
    }

    const auto ts = pickPostingTimestamp(monthStart);
    if (ts >= windowEndEpochSec_) {
      return false;
    }

    const auto amount = sampleAmount();
    if (amount == 0.0) {
      return false;
    }

    out_.push_back(run_.emission().make(transactions::Draft{
        .source = gpAcct,
        .destination = gcAcct,
        .amount = amount,
        .timestamp = ts,
        .isFraud = 0,
        .ringId = -1,
        .channel = channels::tag(channels::Family::grandparentGift),
    }));
    return true;
  }

  [[nodiscard]] std::int64_t
  pickPostingTimestamp(::PhantomLedger::time::TimePoint monthStart) {
    const auto day = rng_.uniformInt(0, kPostingDayMaxExcl);
    const auto hour = rng_.uniformInt(kPostingHourMin, kPostingHourMaxExcl);
    const auto minute = rng_.uniformInt(0, 60);

    const auto base = ::PhantomLedger::time::toEpochSeconds(
        ::PhantomLedger::time::addDays(monthStart, static_cast<int>(day)));
    return base + hour * 3600 + minute * 60;
  }

  [[nodiscard]] double sampleAmount() {
    const auto raw = dist::lognormalByMedian(rng_, cfg_.median, cfg_.sigma);
    return fhelp::sanitizeAmount(raw, kAmountFloor);
  }

  const TransferRun &run_;
  const GrandparentGiftFlow &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

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

  GrandparentGiftEmitter emitter{run, cfg, rng, out};

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
        emitter.processPair(gp, gc);
      }
    }
  }

  return out;
}

} // namespace
  // PhantomLedger::transfers::legit::routines::family::grandparent_gifts
