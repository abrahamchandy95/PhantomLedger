#include "phantomledger/transfers/legit/routines/family/spouse.hpp"

#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

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

/// Stateful, narrow-purpose emitter for spouse-to-spouse transfers.
///
/// Compared to its peers, this emitter has *two* public methods --
/// `resolveCouple` (called once per pair) and `processCoupleMonth`
/// (called once per pair-month). Both share the same dependencies, so
/// holding them as members eliminates the redundant 5- and 7-parameter
/// signatures that the original file used.
class SpouseEmitter {
public:
  SpouseEmitter(const TransferRun &run, const CoupleFlow &cfg, random::Rng &rng,
                std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  SpouseEmitter(const SpouseEmitter &) = delete;
  SpouseEmitter &operator=(const SpouseEmitter &) = delete;

  /// Resolve a candidate spouse pair into separate higher/lower
  /// accounts, applying the `separateAccountsP` gate. Returns
  /// `std::nullopt` if the couple is not eligible (shared account,
  /// both external, gate failed).
  [[nodiscard]] std::optional<CoupleState> resolveCouple(entity::PersonId a,
                                                         entity::PersonId b) {
    if (!rng_.coin(cfg_.separateAccountsP)) {
      return std::nullopt;
    }

    const auto acctA = run_.accounts().routedMemberAccount(a);
    const auto acctB = run_.accounts().routedMemberAccount(b);
    if (!acctA.has_value() || !acctB.has_value() || *acctA == *acctB) {
      return std::nullopt;
    }
    if (encoding::isExternal(*acctA) && encoding::isExternal(*acctB)) {
      return std::nullopt;
    }

    const auto powerA = run_.kinship().amountMultiplier(a);
    const auto powerB = run_.kinship().amountMultiplier(b);

    if (powerA >= powerB) {
      return CoupleState{.higherAcct = *acctA, .lowerAcct = *acctB};
    }
    return CoupleState{.higherAcct = *acctB, .lowerAcct = *acctA};
  }

  /// Emit zero or more transfers for a single (couple, monthStart)
  /// pair. The number of transfers is sampled from the configured
  /// per-month range.
  void processCoupleMonth(const CoupleState &couple,
                          ::PhantomLedger::time::TimePoint monthStart) {
    const auto count = static_cast<int>(
        rng_.uniformInt(cfg_.txnsPerMonthMin,
                        static_cast<std::int64_t>(cfg_.txnsPerMonthMax) + 1));

    for (int i = 0; i < count; ++i) {
      (void)tryEmit(couple, monthStart);
    }
  }

private:
  /// Emit a single spouse transfer. Returns true on success.
  [[nodiscard]] bool tryEmit(const CoupleState &couple,
                             ::PhantomLedger::time::TimePoint monthStart) {
    const bool fromBreadwinner = rng_.coin(cfg_.breadwinnerFlowP);
    const auto src = fromBreadwinner ? couple.higherAcct : couple.lowerAcct;
    const auto dst = fromBreadwinner ? couple.lowerAcct : couple.higherAcct;

    const auto ts = pickPostingTimestamp(monthStart);
    if (ts >= windowEndEpochSec_) {
      return false;
    }

    const auto amount = sampleAmount();
    if (amount == 0.0) {
      return false;
    }

    out_.push_back(run_.emission().make(transactions::Draft{
        .source = src,
        .destination = dst,
        .amount = amount,
        .timestamp = ts,
        .isFraud = 0,
        .ringId = -1,
        .channel = channels::tag(channels::Family::spouseTransfer),
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
    const auto raw =
        dist::lognormalByMedian(rng_, cfg_.transferMedian, cfg_.transferSigma);
    return fhelp::sanitizeAmount(raw, kAmountFloor);
  }

  const TransferRun &run_;
  const CoupleFlow &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const CoupleFlow &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0 || run.posting().monthStarts().empty()) {
    return out;
  }

  auto rng = run.emission().rng({"family", "spouse"});

  const auto avgPerMonth = (cfg.txnsPerMonthMin + cfg.txnsPerMonthMax + 1) / 2;
  out.reserve(static_cast<std::size_t>(personCount / 4U) *
              run.posting().monthStarts().size() *
              static_cast<std::size_t>(std::max(1, avgPerMonth)));

  SpouseEmitter emitter{run, cfg, rng, out};

  for (entity::PersonId p = 1; p <= personCount; ++p) {
    const auto spouse = run.kinship().spouseOf(p);
    if (!entity::valid(spouse) || spouse <= p) {
      continue;
    }

    const auto couple = emitter.resolveCouple(p, spouse);
    if (!couple.has_value()) {
      continue;
    }

    for (const auto monthStart : run.posting().monthStarts()) {
      emitter.processCoupleMonth(*couple, monthStart);
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::spouse
