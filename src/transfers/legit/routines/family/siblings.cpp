#include "phantomledger/transfers/legit/routines/family/siblings.hpp"

#include "phantomledger/entities/encoding/external.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::siblings {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;
namespace dist = ::PhantomLedger::probability::distributions;

namespace {

inline constexpr std::int64_t kPostingDayMaxExcl = 28;
inline constexpr std::int64_t kPostingHourMin = 8;
inline constexpr std::int64_t kPostingHourMaxExcl = 22;
inline constexpr double kAmountFloor = 5.0;

struct ResolvedPair {
  entity::PersonId personA = entity::invalidPerson;
  entity::PersonId personB = entity::invalidPerson;
  entity::Key acctA{};
  entity::Key acctB{};
};

void collectAdults(std::span<const entity::PersonId> members,
                   std::span<const ::PhantomLedger::personas::Type> personas,
                   std::vector<entity::PersonId> &out) {
  out.clear();
  for (const auto p : members) {
    if (pred::isAdult(personas[p - 1])) {
      out.push_back(p);
    }
  }
}

/// Stateful, narrow-purpose emitter for sibling-to-sibling transfers.
///
/// Like `SpouseEmitter`, this exposes two public methods because the
/// resolution step runs once per pair while the emit step runs once
/// per (pair, month). Both share the same dependency views.
class SiblingEmitter {
public:
  SiblingEmitter(const TransferRun &run, const SiblingFlow &cfg,
                 random::Rng &rng,
                 std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  SiblingEmitter(const SiblingEmitter &) = delete;
  SiblingEmitter &operator=(const SiblingEmitter &) = delete;

  /// Process one candidate sibling pair across all month-starts.
  /// Drops out early on the activeP gate, the spouse-of check, or
  /// account-resolution failures.
  void processPair(entity::PersonId a, entity::PersonId b) {
    if (!rng_.coin(cfg_.activeP)) {
      return;
    }

    const auto pair = resolvePair(a, b);
    if (!pair.has_value()) {
      return;
    }

    for (const auto monthStart : run_.posting().monthStarts()) {
      (void)tryEmitMonthly(*pair, monthStart);
    }
  }

private:
  [[nodiscard]] std::optional<ResolvedPair> resolvePair(entity::PersonId a,
                                                        entity::PersonId b) {
    if (run_.kinship().spouseOf(a) == b) {
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

    return ResolvedPair{
        .personA = a, .personB = b, .acctA = *acctA, .acctB = *acctB};
  }

  /// Try to emit one monthly transfer for this pair. Returns true on
  /// success.
  [[nodiscard]] bool
  tryEmitMonthly(const ResolvedPair &pair,
                 ::PhantomLedger::time::TimePoint monthStart) {
    if (!rng_.coin(cfg_.monthlyP)) {
      return false;
    }

    const bool aToB = rng_.coin(0.5);
    const auto src = aToB ? pair.acctA : pair.acctB;
    const auto dst = aToB ? pair.acctB : pair.acctA;

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
        .channel = channels::tag(channels::Family::siblingTransfer),
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
  const SiblingFlow &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const SiblingFlow &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  const auto householdCount = run.kinship().householdCount();
  if (householdCount == 0 || run.posting().monthStarts().empty()) {
    return out;
  }

  auto rng = run.emission().rng({"family", "siblings"});

  SiblingEmitter emitter{run, cfg, rng, out};

  std::vector<entity::PersonId> adults;
  adults.reserve(8);

  for (std::uint32_t h = 0; h < householdCount; ++h) {
    const auto members = run.kinship().householdMembersOf(h);
    collectAdults(members, run.kinship().personas(), adults);

    if (adults.size() < 2) {
      continue;
    }

    for (std::size_t i = 0; i < adults.size(); ++i) {
      for (std::size_t j = i + 1; j < adults.size(); ++j) {
        emitter.processPair(adults[i], adults[j]);
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::siblings
