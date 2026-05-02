#include "phantomledger/transfers/legit/routines/family/allowances.hpp"

#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <array>
#include <cstdint>

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

[[nodiscard]] bool emitOnePayment(entity::PersonId studentId,
                                  entity::Key childAcct,
                                  std::span<const entity::PersonId> parents,
                                  std::int64_t timestamp,
                                  const AllowanceSchedule &cfg,
                                  const Runtime &rt, random::Rng &rng,
                                  std::vector<transactions::Transaction> &out) {
  if (parents.empty()) {
    return false;
  }

  const auto parentIdx = static_cast<std::size_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(parents.size())));
  const auto payerId = parents[parentIdx];

  const auto payerAcct = fhelp::resolveFamilyAccount(
      payerId, *rt.accounts, *rt.ownership, rt.routing.externalP);
  if (!payerAcct.has_value() || *payerAcct == childAcct) {
    return false;
  }

  const auto rawAmt = fhelp::pareto(rng, cfg.paretoXm, cfg.paretoAlpha);
  const auto amt = fhelp::sanitizeAmount(rawAmt, kAmountFloor);
  if (amt == 0.0) {
    return false;
  }

  out.push_back(rt.txf->make(transactions::Draft{
      .source = *payerAcct,
      .destination = childAcct,
      .amount = amt,
      .timestamp = timestamp,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::allowance),
  }));
  (void)studentId;
  return true;
}

void walkSchedule(entity::PersonId studentId, entity::Key childAcct,
                  std::span<const entity::PersonId> parents,
                  const Schedule &schedule, std::int64_t windowEndEpochSec,
                  const AllowanceSchedule &cfg, const Runtime &rt,
                  random::Rng &rng,
                  std::vector<transactions::Transaction> &out) {
  std::int64_t ts = schedule.firstTs;

  while (ts < windowEndEpochSec) {
    (void)emitOnePayment(studentId, childAcct, parents, ts, cfg, rt, rng, out);

    const auto jitter = rng.uniformInt(kJitterMin, kJitterMaxExcl);
    const auto stepDays =
        static_cast<std::int64_t>(schedule.intervalDays) + jitter;
    ts += stepDays * kSecondsPerDay;
  }
}

[[nodiscard]] std::size_t estimateCapacity(std::uint32_t studentCount,
                                           int windowDays) {
  if (studentCount == 0 || windowDays <= 0) {
    return 0;
  }
  const auto perStudent = static_cast<std::size_t>(windowDays) / 7U;
  // Approx 80% of the weekly upper bound.
  return (studentCount * perStudent * 4U) / 5U;
}

} // namespace

std::vector<transactions::Transaction> generate(const Runtime &rt,
                                                const AllowanceSchedule &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || rt.graph == nullptr || rt.accounts == nullptr ||
      rt.ownership == nullptr || rt.txf == nullptr ||
      rt.rngFactory == nullptr) {
    return out;
  }

  const auto personCount = rt.graph->personCount();
  if (personCount == 0) {
    return out;
  }

  out.reserve(estimateCapacity(personCount, rt.window.days));

  auto rng = rt.rngFactory->rng({"family", "allowances"});

  const auto windowEndEpochSec =
      ::PhantomLedger::time::toEpochSeconds(rt.window.endExcl());

  for (entity::PersonId student = 1; student <= personCount; ++student) {
    if (!pred::isStudent(rt.personas[student - 1])) {
      continue;
    }

    // Parents stored as fixed-2 array; find the populated prefix.
    const auto parentSlots = rt.graph->parentsOf[student - 1];
    std::array<entity::PersonId, 2> parentBuf{};
    std::size_t parentCount = 0;
    for (const auto p : parentSlots) {
      if (entity::valid(p)) {
        parentBuf[parentCount++] = p;
      }
    }
    if (parentCount == 0) {
      continue;
    }

    // Resolve student's primary account once.
    const auto childAcct = fhelp::resolveFamilyAccount(
        student, *rt.accounts, *rt.ownership, /*externalP=*/0.0);
    if (!childAcct.has_value()) {
      continue;
    }

    const auto schedule = pickSchedule(rng, rt.window.start, cfg.weeklyP);
    walkSchedule(
        student, *childAcct,
        std::span<const entity::PersonId>{parentBuf.data(), parentCount},
        schedule, windowEndEpochSec, cfg, rt, rng, out);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::allowances
