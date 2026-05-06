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

[[nodiscard]] std::size_t estimateCapacity(std::size_t retireeCount,
                                           std::size_t monthCount,
                                           double supportP) noexcept {
  if (retireeCount == 0 || monthCount == 0) {
    return 0;
  }
  const auto expected =
      static_cast<double>(retireeCount * monthCount) * supportP;
  return static_cast<std::size_t>(expected) + (retireeCount / 4U);
}

/// Stateful, narrow-purpose emitter for child-to-retiree support
/// transactions. Same pattern as `AllowanceEmitter`: dependency views
/// and sinks are members; only routine-shape arguments cross method
/// boundaries.
class SupportEmitter {
public:
  SupportEmitter(const TransferRun &run, const SupportFlow &cfg,
                 random::Rng &rng,
                 std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  SupportEmitter(const SupportEmitter &) = delete;
  SupportEmitter &operator=(const SupportEmitter &) = delete;

  /// Try to emit a single monthly support transfer from a weighted-
  /// pick supporter into the retiree's account. Returns true on
  /// success.
  [[nodiscard]] bool tryEmit(entity::Key retireeAcct,
                             std::span<const entity::PersonId> supporters,
                             ::PhantomLedger::time::TimePoint monthStart) {
    if (!rng_.coin(cfg_.supportP)) {
      return false;
    }

    const auto payer = resolvePayer(supporters, retireeAcct);
    if (!payer.has_value()) {
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
        .destination = retireeAcct,
        .amount = amount,
        .timestamp = ts,
        .isFraud = 0,
        .ringId = -1,
        .channel = channels::tag(channels::Family::support),
    }));
    return true;
  }

private:
  struct Payer {
    entity::PersonId person;
    entity::Key account;
  };

  [[nodiscard]] std::optional<Payer>
  resolvePayer(std::span<const entity::PersonId> supporters,
               entity::Key retireeAcct) {
    const auto picked = fhelp::weightedPickPerson(
        supporters, run_.kinship().amountMultipliers(), rng_);
    if (!entity::valid(picked)) {
      return std::nullopt;
    }

    const auto payerAcct = run_.accounts().routedMemberAccount(picked);
    if (!payerAcct.has_value() || *payerAcct == retireeAcct) {
      return std::nullopt;
    }

    return Payer{.person = picked, .account = *payerAcct};
  }

  [[nodiscard]] double sampleAmount(entity::PersonId payer) {
    const auto base = fhelp::pareto(rng_, cfg_.paretoXm, cfg_.paretoAlpha);
    const auto multiplier = run_.kinship().amountMultiplier(payer);
    return fhelp::sanitizeAmount(base * multiplier, kAmountFloor);
  }

  const TransferRun &run_;
  const SupportFlow &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

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

  SupportEmitter emitter{run, cfg, rng, out};

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
      (void)emitter.tryEmit(*retireeAcct, supportersView, monthStart);
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::support
