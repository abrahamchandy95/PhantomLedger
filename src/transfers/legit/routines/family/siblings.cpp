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

[[nodiscard]] std::optional<ResolvedPair>
resolvePair(entity::PersonId a, entity::PersonId b, const TransferRun &run) {
  if (run.kinship().spouseOf(a) == b) {
    return std::nullopt;
  }

  const auto acctA = run.accounts().routedMemberAccount(a);
  const auto acctB = run.accounts().routedMemberAccount(b);
  if (!acctA.has_value() || !acctB.has_value() || *acctA == *acctB) {
    return std::nullopt;
  }

  if (encoding::isExternal(*acctA) && encoding::isExternal(*acctB)) {
    return std::nullopt;
  }

  return ResolvedPair{
      .personA = a, .personB = b, .acctA = *acctA, .acctB = *acctB};
}

[[nodiscard]] bool
emitMonthlyTransfer(::PhantomLedger::time::TimePoint monthStart,
                    const ResolvedPair &pair, std::int64_t windowEndEpochSec,
                    const SiblingFlow &cfg, const TransferRun &run,
                    random::Rng &rng,
                    std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.monthlyP)) {
    return false;
  }

  const bool aToB = rng.coin(0.5);
  const auto src = aToB ? pair.acctA : pair.acctB;
  const auto dst = aToB ? pair.acctB : pair.acctA;

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
      .source = src,
      .destination = dst,
      .amount = amt,
      .timestamp = ts,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::siblingTransfer),
  }));
  return true;
}

void processPair(entity::PersonId a, entity::PersonId b,
                 std::int64_t windowEndEpochSec, const SiblingFlow &cfg,
                 const TransferRun &run, random::Rng &rng,
                 std::vector<transactions::Transaction> &out) {
  if (!rng.coin(cfg.activeP)) {
    return;
  }

  const auto pair = resolvePair(a, b, run);
  if (!pair.has_value()) {
    return;
  }

  for (const auto monthStart : run.posting().monthStarts()) {
    (void)emitMonthlyTransfer(monthStart, *pair, windowEndEpochSec, cfg, run,
                              rng, out);
  }
}

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

  const auto windowEndEpochSec = run.posting().endEpochSec();

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
        processPair(adults[i], adults[j], windowEndEpochSec, cfg, run, rng,
                    out);
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::siblings
