#include "phantomledger/transfers/legit/routines/family/parent_gifts.hpp"

#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::parent_gifts {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;

namespace {

inline constexpr std::int64_t kPostingDayMaxExcl = 20;
inline constexpr std::int64_t kPostingHourMin = 8;
inline constexpr std::int64_t kPostingHourMaxExcl = 21;
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

/// Stateful, narrow-purpose emitter for parent-gift transactions.
/// Same pattern as `AllowanceEmitter`: holds the dependency views and
/// the output sink so internal helpers stop carrying eight parameters.
class ParentGiftEmitter {
public:
  ParentGiftEmitter(const TransferRun &run, const ParentGiftFlow &cfg,
                    random::Rng &rng,
                    std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  ParentGiftEmitter(const ParentGiftEmitter &) = delete;
  ParentGiftEmitter &operator=(const ParentGiftEmitter &) = delete;

  /// Try to emit one monthly gift from a randomly-picked giver to the
  /// child's account. Returns true on success.
  [[nodiscard]] bool tryEmit(entity::Key childAcct,
                             std::span<const entity::PersonId> givingParents,
                             ::PhantomLedger::time::TimePoint monthStart) {
    if (!rng_.coin(cfg_.p)) {
      return false;
    }

    const auto payer = pickPayer(givingParents);
    if (!payer.has_value() || payer->account == childAcct) {
      return false;
    }

    const auto ts = pickPostingTimestamp(monthStart, rng_);
    if (ts >= windowEndEpochSec_) {
      return false;
    }

    const auto amount = sampleAmount(payer->person);
    if (amount == 0.0) {
      return false;
    }

    out_.push_back(run_.emission().make(transactions::Draft{
        .source = payer->account,
        .destination = childAcct,
        .amount = amount,
        .timestamp = ts,
        .isFraud = 0,
        .ringId = -1,
        .channel = channels::tag(channels::Family::parentGift),
    }));
    return true;
  }

private:
  struct Payer {
    entity::PersonId person;
    entity::Key account;
  };

  [[nodiscard]] std::optional<Payer>
  pickPayer(std::span<const entity::PersonId> givingParents) {
    if (givingParents.empty()) {
      return std::nullopt;
    }

    const auto idx = static_cast<std::size_t>(
        rng_.uniformInt(0, static_cast<std::int64_t>(givingParents.size())));
    const auto person = givingParents[idx];

    const auto acct = run_.accounts().routedMemberAccount(person);
    if (!acct.has_value()) {
      return std::nullopt;
    }

    return Payer{.person = person, .account = *acct};
  }

  [[nodiscard]] double sampleAmount(entity::PersonId payer) {
    const auto base = fhelp::pareto(rng_, cfg_.paretoXm, cfg_.paretoAlpha);
    const auto multiplier = run_.kinship().amountMultiplier(payer);
    return fhelp::sanitizeAmount(base * multiplier, kAmountFloor);
  }

  const TransferRun &run_;
  const ParentGiftFlow &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const ParentGiftFlow &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0 || run.posting().monthStarts().empty()) {
    return out;
  }

  auto rng = run.emission().rng({"family", "parent_gifts"});

  out.reserve(static_cast<std::size_t>(
      static_cast<double>(personCount) *
      static_cast<double>(run.posting().monthStarts().size()) * cfg.p * 0.1));

  ParentGiftEmitter emitter{run, cfg, rng, out};

  std::array<entity::PersonId, 2> givingParents{};

  for (entity::PersonId child = 1; child <= personCount; ++child) {
    if (!pred::isAdult(run.kinship().persona(child))) {
      continue;
    }

    const auto parentSlots = run.kinship().parentsOf(child);
    if (!entity::valid(parentSlots[0]) && !entity::valid(parentSlots[1])) {
      continue;
    }

    const auto givingCount = collectGivingParents(
        std::span<const entity::PersonId>{parentSlots.data(),
                                          parentSlots.size()},
        run.kinship().personas(), givingParents);
    if (givingCount == 0) {
      continue;
    }

    const auto childAcct = run.accounts().localMemberAccount(child);
    if (!childAcct.has_value()) {
      continue;
    }

    const std::span<const entity::PersonId> givingView{givingParents.data(),
                                                       givingCount};
    for (const auto monthStart : run.posting().monthStarts()) {
      (void)emitter.tryEmit(*childAcct, givingView, monthStart);
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::parent_gifts
