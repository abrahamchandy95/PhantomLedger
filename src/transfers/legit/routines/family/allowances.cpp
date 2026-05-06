#include "phantomledger/transfers/legit/routines/family/allowances.hpp"

#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace PhantomLedger::transfers::legit::routines::family::allowances {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;

namespace {

inline constexpr int kWeeklyDays = 7;
inline constexpr int kBiweeklyDays = 14;

inline constexpr std::int64_t kSeedDayMin = 0;
inline constexpr std::int64_t kSeedDayMax = 14;

inline constexpr std::int64_t kSeedHourMin = 7;
inline constexpr std::int64_t kSeedHourMax = 21;

inline constexpr std::int64_t kJitterMin = -1;
inline constexpr std::int64_t kJitterMaxExcl = 2;

inline constexpr double kAmountFloor = 1.0;

inline constexpr std::int64_t kSecondsPerDay = 86400;

struct Schedule {
  std::int64_t firstTs = 0;
  int intervalDays = 0;
};

[[nodiscard]] Schedule
pickSchedule(random::Rng &rng, ::PhantomLedger::time::TimePoint windowStart,
             double weeklyP) {
  const bool weekly = rng.coin(weeklyP);

  const auto seedDay = rng.uniformInt(kSeedDayMin, kSeedDayMax);
  const auto seedHour = rng.uniformInt(kSeedHourMin, kSeedHourMax);
  const auto seedMin = rng.uniformInt(0, 60);

  const auto base = ::PhantomLedger::time::toEpochSeconds(
      ::PhantomLedger::time::addDays(windowStart, static_cast<int>(seedDay)));
  const auto firstTs = base + seedHour * 3600 + seedMin * 60;

  return Schedule{
      .firstTs = firstTs,
      .intervalDays = weekly ? kWeeklyDays : kBiweeklyDays,
  };
}

[[nodiscard]] std::size_t
collectValidParents(std::array<entity::PersonId, 2> parentSlots,
                    std::array<entity::PersonId, 2> &out) noexcept {
  std::size_t n = 0;
  for (const auto p : parentSlots) {
    if (entity::valid(p)) {
      out[n++] = p;
    }
  }
  return n;
}

[[nodiscard]] std::size_t estimateCapacity(std::uint32_t studentCount,
                                           int windowDays) noexcept {
  if (studentCount == 0 || windowDays <= 0) {
    return 0;
  }
  const auto perStudent = static_cast<std::size_t>(windowDays) / 7U;
  return (studentCount * perStudent * 4U) / 5U;
}

/// Stateful, narrow-purpose emitter for allowance transactions over one
/// posting window. Owns the dependency views and the output sink so that
/// internal helpers do not have to thread eight unrelated arguments
/// through every call.
///
/// This is *not* a generic "Context" or "Bundle" object:
///   - it is named for the verb it performs (emit allowance txns);
///   - it has a single reason to change (allowance emission semantics);
///   - its public surface is one method (`walkSchedule`);
///   - its fields are the dependencies of *that one job*, not a
///     heterogeneous collection.
///
/// It mirrors the standard pattern used by LLVM's IRBuilder, Boost.Beast
/// stream emitters, and gRPC writers: a small, locally-scoped class that
/// holds the cursor + sinks for one coherent emission task.
class AllowanceEmitter {
public:
  AllowanceEmitter(const TransferRun &run, const AllowanceSchedule &cfg,
                   random::Rng &rng,
                   std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  AllowanceEmitter(const AllowanceEmitter &) = delete;
  AllowanceEmitter &operator=(const AllowanceEmitter &) = delete;

  /// Walk the schedule for a single student, emitting zero or more
  /// payments until the posting window ends.
  void walkSchedule(entity::Key childAcct,
                    std::span<const entity::PersonId> parents,
                    Schedule schedule) {
    std::int64_t ts = schedule.firstTs;

    while (ts < windowEndEpochSec_) {
      (void)tryEmit(childAcct, parents, ts);

      const auto jitter = rng_.uniformInt(kJitterMin, kJitterMaxExcl);
      const auto stepDays =
          static_cast<std::int64_t>(schedule.intervalDays) + jitter;
      ts += stepDays * kSecondsPerDay;
    }
  }

private:
  /// Emit a single allowance payment from a randomly-picked parent to
  /// the child's account. Returns true on success.
  [[nodiscard]] bool tryEmit(entity::Key childAcct,
                             std::span<const entity::PersonId> parents,
                             std::int64_t timestamp) {
    const auto payerAcct = pickPayer(parents);
    if (!payerAcct.has_value() || *payerAcct == childAcct) {
      return false;
    }

    const auto amount = sampleAmount();
    if (amount == 0.0) {
      return false;
    }

    out_.push_back(run_.emission().make(transactions::Draft{
        .source = *payerAcct,
        .destination = childAcct,
        .amount = amount,
        .timestamp = timestamp,
        .isFraud = 0,
        .ringId = -1,
        .channel = channels::tag(channels::Family::allowance),
    }));
    return true;
  }

  [[nodiscard]] std::optional<entity::Key>
  pickPayer(std::span<const entity::PersonId> parents) {
    if (parents.empty()) {
      return std::nullopt;
    }

    const auto parentIdx = static_cast<std::size_t>(
        rng_.uniformInt(0, static_cast<std::int64_t>(parents.size())));
    const auto payerId = parents[parentIdx];

    return run_.accounts().routedMemberAccount(payerId);
  }

  [[nodiscard]] double sampleAmount() {
    const auto raw = fhelp::pareto(rng_, cfg_.paretoXm, cfg_.paretoAlpha);
    return fhelp::sanitizeAmount(raw, kAmountFloor);
  }

  const TransferRun &run_;
  const AllowanceSchedule &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const AllowanceSchedule &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0) {
    return out;
  }

  out.reserve(estimateCapacity(personCount, run.posting().days()));

  auto rng = run.emission().rng({"family", "allowances"});

  AllowanceEmitter emitter{run, cfg, rng, out};

  std::array<entity::PersonId, 2> parentBuf{};

  for (entity::PersonId student = 1; student <= personCount; ++student) {
    if (!pred::isStudent(run.kinship().persona(student))) {
      continue;
    }

    const auto parentCount =
        collectValidParents(run.kinship().parentsOf(student), parentBuf);
    if (parentCount == 0) {
      continue;
    }

    const auto childAcct = run.accounts().localMemberAccount(student);
    if (!childAcct.has_value()) {
      continue;
    }

    const auto schedule = pickSchedule(rng, run.posting().start(), cfg.weeklyP);

    emitter.walkSchedule(
        *childAcct,
        std::span<const entity::PersonId>{parentBuf.data(), parentCount},
        schedule);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::allowances
