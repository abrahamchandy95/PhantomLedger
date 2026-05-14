#include "phantomledger/transfers/legit/routines/family/tuition.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/distributions/normal.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

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

struct InstallmentPlan {
  int installments = 0;
  double installmentBase = 0.0;
  std::int64_t semesterStartEpoch = 0;
};

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

class TuitionEmitter {
public:
  TuitionEmitter(const TransferRun &run, const TuitionSchedule &cfg,
                 random::Rng &rng,
                 std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  TuitionEmitter(const TuitionEmitter &) = delete;
  TuitionEmitter &operator=(const TuitionEmitter &) = delete;

  void processStudent(std::span<const entity::PersonId> parents,
                      entity::Key payeeAcct) {
    if (!rng_.coin(cfg_.p)) {
      return;
    }

    const auto payerAcct = pickPayer(parents, payeeAcct);
    if (!payerAcct.has_value()) {
      return;
    }

    if (run_.posting().monthStarts().empty()) {
      return;
    }

    const auto plan = buildPlan(run_.posting().firstMonthStart());

    for (int i = 0; i < plan.installments; ++i) {
      if (!tryEmitInstallment(i, plan, *payerAcct, payeeAcct)) {
        break;
      }
    }
  }

private:
  [[nodiscard]] std::optional<entity::Key>
  pickPayer(std::span<const entity::PersonId> parents, entity::Key payeeAcct) {
    if (parents.empty()) {
      return std::nullopt;
    }

    const auto parentIdx = static_cast<std::size_t>(
        rng_.uniformInt(0, static_cast<std::int64_t>(parents.size())));
    const auto payerId = parents[parentIdx];

    const auto payerAcct = run_.accounts().localMemberAccount(payerId);
    if (!payerAcct.has_value() || *payerAcct == payeeAcct) {
      return std::nullopt;
    }
    return *payerAcct;
  }

  [[nodiscard]] InstallmentPlan
  buildPlan(::PhantomLedger::time::TimePoint firstMonthStart) {
    const auto count = static_cast<int>(rng_.uniformInt(
        cfg_.instMin, static_cast<std::int64_t>(cfg_.instMax) + 1));

    const auto rawTotal = dist::lognormal(rng_, cfg_.mu, cfg_.sigma);
    const auto total = std::max(kTotalTuitionFloor, rawTotal);

    const auto base = primitives::utils::roundMoney(
        total / static_cast<double>(std::max(1, count)));

    const auto offsetDays = rng_.uniformInt(0, kSemesterStartJitterDaysExcl);
    const auto anchorEpoch =
        ::PhantomLedger::time::toEpochSeconds(::PhantomLedger::time::addDays(
            firstMonthStart, static_cast<int>(offsetDays)));

    return InstallmentPlan{
        .installments = count,
        .installmentBase = base,
        .semesterStartEpoch = anchorEpoch,
    };
  }

  [[nodiscard]] bool tryEmitInstallment(int stepIndex,
                                        const InstallmentPlan &plan,
                                        entity::Key payerAcct,
                                        entity::Key payeeAcct) {
    const auto baseTs =
        plan.semesterStartEpoch + static_cast<std::int64_t>(stepIndex) *
                                      kInstallmentSpacingDays *
                                      ::PhantomLedger::time::kSecondsPerDay;
    if (baseTs >= windowEndEpochSec_) {
      return false;
    }

    const auto jitterDay = rng_.uniformInt(0, kInstallmentDayJitterExcl);
    const auto jitterHour =
        rng_.uniformInt(kInstallmentHourMin, kInstallmentHourMaxExcl);
    const auto jitterMin = rng_.uniformInt(0, 60);

    const auto ts = baseTs + jitterDay * ::PhantomLedger::time::kSecondsPerDay +
                    ::PhantomLedger::time::secondsInDay(jitterHour, jitterMin);
    if (ts >= windowEndEpochSec_) {
      return false;
    }

    const auto noise = kInstallmentNoiseSigma * dist::standardNormal(rng_);
    const auto amt = fhelp::sanitizeAmount(plan.installmentBase * (1.0 + noise),
                                           kInstallmentAmountFloor);
    if (amt == 0.0) {
      return false;
    }

    out_.push_back(run_.emission().make(transactions::Draft{
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

  const TransferRun &run_;
  const TuitionSchedule &cfg_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
};

} // namespace

std::vector<transactions::Transaction> generate(const TransferRun &run,
                                                const TuitionSchedule &cfg) {
  std::vector<transactions::Transaction> out;
  if (!cfg.enabled || !run.ready() || !run.education().ready()) {
    return out;
  }

  const auto personCount = run.kinship().personCount();
  if (personCount == 0) {
    return out;
  }

  auto rng = run.emission().rng({"family", "tuition"});

  const auto payeeAcct = run.education().pick(rng);
  if (!payeeAcct.has_value()) {
    return out;
  }

  std::size_t studentCount = 0;
  for (entity::PersonId p = 1; p <= personCount; ++p) {
    if (pred::isStudent(run.kinship().persona(p))) {
      ++studentCount;
    }
  }
  out.reserve(estimateCapacity(studentCount, cfg));

  TuitionEmitter emitter{run, cfg, rng, out};

  for (entity::PersonId student = 1; student <= personCount; ++student) {
    if (!pred::isStudent(run.kinship().persona(student))) {
      continue;
    }

    const auto parentSlots = run.kinship().parentsOf(student);
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

    emitter.processStudent(
        std::span<const entity::PersonId>{parentBuf.data(), parentCount},
        *payeeAcct);
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::tuition
