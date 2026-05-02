#include "phantomledger/transfers/legit/routines/family/tuition.hpp"

#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/probability/distributions/normal.hpp"
#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace PhantomLedger::transfers::legit::routines::family::tuition {

namespace pred = ::PhantomLedger::relationships::family::predicates;
namespace fhelp = ::PhantomLedger::transfers::legit::routines::family::helpers;
namespace dist = ::PhantomLedger::probability::distributions;

namespace {

inline constexpr int kInstallmentSpacingDays = 30;

inline constexpr double kTotalTuitionFloor = 200.0;

inline constexpr double kInstallmentAmountFloor = 10.0;

inline constexpr double kInstallmentNoiseSigma = 0.03;

inline constexpr std::int64_t kSemesterStartJitterDaysExcl = 10;

inline constexpr std::int64_t kInstallmentDayJitterExcl = 5;
inline constexpr std::int64_t kInstallmentHourMin = 8;
inline constexpr std::int64_t kInstallmentHourMaxExcl = 18;

struct Plan {
  int installments = 0;
  double installmentBase = 0.0;
  std::int64_t semesterStartEpoch = 0;
};

[[nodiscard]] Plan buildPlan(const TuitionSchedule &cfg,
                             ::PhantomLedger::time::TimePoint firstMonthStart,
                             random::Rng &rng) {
  const auto count = static_cast<int>(
      rng.uniformInt(cfg.instMin, static_cast<std::int64_t>(cfg.instMax) + 1));

  const auto rawTotal = dist::lognormal(rng, cfg.mu, cfg.sigma);
  const auto total = std::max(kTotalTuitionFloor, rawTotal);

  const auto base =
      fhelp::roundCents(total / static_cast<double>(std::max(1, count)));

  // Anchor — first monthStart plus a few days of jitter.
  const auto offsetDays = rng.uniformInt(0, kSemesterStartJitterDaysExcl);
  const auto anchorEpoch =
      ::PhantomLedger::time::toEpochSeconds(::PhantomLedger::time::addDays(
          firstMonthStart, static_cast<int>(offsetDays)));

  return Plan{
      .installments = count,
      .installmentBase = base,
      .semesterStartEpoch = anchorEpoch,
  };
}

[[nodiscard]] bool
emitInstallment(int stepIndex, const Plan &plan, entity::Key payerAcct,
                entity::Key payeeAcct, std::int64_t windowEndEpochSec,
                const Runtime &rt, random::Rng &rng,
                std::vector<transactions::Transaction> &out) {
  const auto baseTs =
      plan.semesterStartEpoch +
      static_cast<std::int64_t>(stepIndex) * kInstallmentSpacingDays * 86400;
  if (baseTs >= windowEndEpochSec) {
    return false;
  }

  const auto jitterDay = rng.uniformInt(0, kInstallmentDayJitterExcl);
  const auto jitterHour =
      rng.uniformInt(kInstallmentHourMin, kInstallmentHourMaxExcl);
  const auto jitterMin = rng.uniformInt(0, 60);

  const auto ts =
      baseTs + jitterDay * 86400 + jitterHour * 3600 + jitterMin * 60;
  if (ts >= windowEndEpochSec) {
    return false;
  }

  const auto noise = kInstallmentNoiseSigma * dist::standardNormal(rng);
  const auto amt = fhelp::sanitizeAmount(plan.installmentBase * (1.0 + noise),
                                         kInstallmentAmountFloor);
  if (amt == 0.0) {
    return false;
  }

  out.push_back(rt.txf->make(transactions::Draft{
      .source = payerAcct,
      .destination = payeeAcct,
      .amount = amt,
      .timestamp = ts,
      .isFraud = 0,
      .ringId = -1,
      .channel = channels::tag(channels::Family::tuition),
  }));
  return true;
}

void processStudent(entity::PersonId student,
                    std::span<const entity::PersonId> parents,
                    entity::Key payeeAcct, const TuitionSchedule &cfg,
                    std::int64_t windowEndEpochSec, const Runtime &rt,
                    random::Rng &rng,
                    std::vector<transactions::Transaction> &out) {
  // Per-student gate.
  if (!rng.coin(cfg.p)) {
    return;
  }

  // Pick one parent uniformly.
  const auto parentIdx = static_cast<std::size_t>(
      rng.uniformInt(0, static_cast<std::int64_t>(parents.size())));
  const auto payerId = parents[parentIdx];

  const auto payerAcct = fhelp::resolveFamilyAccount(
      payerId, *rt.accounts, *rt.ownership, /*externalP=*/0.0);
  if (!payerAcct.has_value() || *payerAcct == payeeAcct) {
    return;
  }

  if (rt.monthStarts.empty()) {
    return;
  }

  const auto plan = buildPlan(cfg, rt.monthStarts.front(), rng);

  for (int i = 0; i < plan.installments; ++i) {
    if (!emitInstallment(i, plan, *payerAcct, payeeAcct, windowEndEpochSec, rt,
                         rng, out)) {
      // Once we drift past the window, all later installments
      // would too — short-circuit.
      break;
    }
    (void)student;
  }
}
[[nodiscard]] std::size_t estimateCapacity(std::size_t studentCount,
                                           const TuitionSchedule &cfg) {
  if (studentCount == 0) {
    return 0;
  }
  const auto avgInstallments =
      (static_cast<double>(cfg.instMin) + static_cast<double>(cfg.instMax)) /
      2.0;
  return static_cast<std::size_t>(static_cast<double>(studentCount) *
                                  avgInstallments * cfg.p);
}

} // namespace

std::vector<transactions::Transaction> generate(const Runtime &rt,
                                                const TuitionSchedule &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || rt.graph == nullptr || rt.accounts == nullptr ||
      rt.ownership == nullptr || rt.txf == nullptr ||
      rt.rngFactory == nullptr || rt.merchants == nullptr) {
    return out;
  }

  const auto personCount = rt.graph->personCount();
  if (personCount == 0) {
    return out;
  }

  auto rng = rt.rngFactory->rng({"family", "tuition"});

  const auto payeeAcct = fhelp::pickEducationMerchant(*rt.merchants, rng);
  if (!payeeAcct.has_value()) {
    return out;
  }

  // Estimate capacity from the student count.
  std::size_t studentCount = 0;
  for (entity::PersonId p = 1; p <= personCount; ++p) {
    if (pred::isStudent(rt.personas[p - 1])) {
      ++studentCount;
    }
  }
  out.reserve(estimateCapacity(studentCount, cfg));

  const auto windowEndEpochSec =
      ::PhantomLedger::time::toEpochSeconds(rt.window.endExcl());

  for (entity::PersonId student = 1; student <= personCount; ++student) {
    if (!pred::isStudent(rt.personas[student - 1])) {
      continue;
    }

    // Build the populated parent prefix.
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

    processStudent(
        student,
        std::span<const entity::PersonId>{parentBuf.data(), parentCount},
        *payeeAcct, cfg, windowEndEpochSec, rt, rng, out);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::tuition
