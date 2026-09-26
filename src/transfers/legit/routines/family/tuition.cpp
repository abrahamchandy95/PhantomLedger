#include "phantomledger/transfers/legit/routines/family/tuition.hpp"

#include "phantomledger/primitives/random/distributions/lognormal.hpp"
#include "phantomledger/primitives/random/distributions/normal.hpp"
#include "phantomledger/primitives/time/constants.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/relationships/family/predicates.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"
#include "phantomledger/transfers/legit/routines/family/helpers.hpp"
#include "phantomledger/transfers/legit/routines/family/schools.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

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

struct Installment {
  std::int64_t timestamp = 0;
  double amount = 0.0;
};

// The student's own school lane: {"family", "tuition-school", PersonId}.
[[nodiscard]] random::Rng schoolLane(const TransferEmission &emission,
                                     entity::PersonId student) {
  std::array<char, 16> buf{};
  const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(),
                                       static_cast<unsigned>(student));
  (void)ec;
  const std::string_view id{buf.data(),
                            static_cast<std::size_t>(end - buf.data())};
  return emission.rng({"family", "tuition-school", id});
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

class TuitionEmitter {
public:
  TuitionEmitter(const TransferRun &run, const TuitionSchedule &cfg,
                 const schools::Directory &schools, random::Rng &rng,
                 std::vector<transactions::Transaction> &out) noexcept
      : run_(run), cfg_(cfg), schools_(schools), rng_(rng), out_(out),
        windowEndEpochSec_(run.posting().endEpochSec()) {}

  TuitionEmitter(const TuitionEmitter &) = delete;
  TuitionEmitter &operator=(const TuitionEmitter &) = delete;

  void processStudent(entity::PersonId student,
                      std::span<const entity::PersonId> parents) {
    if (!rng_.coin(cfg_.p)) {
      return;
    }

    const auto payerAcct = pickPayer(parents);
    if (!payerAcct.has_value()) {
      return;
    }

    if (run_.posting().monthStarts().empty()) {
      return;
    }

    const auto plan = buildPlan(run_.posting().firstMonthStart());

    installments_.clear();
    for (int i = 0; i < plan.installments; ++i) {
      const auto installment = drawInstallment(i, plan);
      if (!installment.has_value()) {
        break;
      }
      installments_.push_back(*installment);
    }

    emitPlan(student, *payerAcct);
  }

private:
  [[nodiscard]] std::optional<entity::Key>
  pickPayer(std::span<const entity::PersonId> parents) {
    if (parents.empty()) {
      return std::nullopt;
    }

    const auto parentIdx = static_cast<std::size_t>(
        rng_.uniformInt(0, static_cast<std::int64_t>(parents.size())));
    const auto payerId = parents[parentIdx];

    return run_.accounts().localMemberAccount(payerId);
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

  [[nodiscard]] std::optional<Installment>
  drawInstallment(int stepIndex, const InstallmentPlan &plan) {
    const auto baseTs =
        plan.semesterStartEpoch + static_cast<std::int64_t>(stepIndex) *
                                      kInstallmentSpacingDays *
                                      ::PhantomLedger::time::kSecondsPerDay;
    if (baseTs >= windowEndEpochSec_) {
      return std::nullopt;
    }

    const auto jitterDay = rng_.uniformInt(0, kInstallmentDayJitterExcl);
    const auto jitterHour =
        rng_.uniformInt(kInstallmentHourMin, kInstallmentHourMaxExcl);
    const auto jitterMin = rng_.uniformInt(0, 60);

    const auto ts = baseTs + jitterDay * ::PhantomLedger::time::kSecondsPerDay +
                    ::PhantomLedger::time::secondsInDay(jitterHour, jitterMin);
    if (ts >= windowEndEpochSec_) {
      return std::nullopt;
    }

    const auto noise = kInstallmentNoiseSigma * dist::standardNormal(rng_);
    const auto amt = fhelp::sanitizeAmount(plan.installmentBase * (1.0 + noise),
                                           kInstallmentAmountFloor);
    if (amt == 0.0) {
      return std::nullopt;
    }

    // H1 step 2b (class P): calibration-year draw realized at the
    // installment date's CPI level.
    return Installment{.timestamp = ts, .amount = fhelp::nominalAt(amt, ts)};
  }

  /* The plan's rows, all to the student's one school (tuition-payee-2026-09).
   *
   * The school is picked once the rows are drawn, because it must be open on
   * every installment date; it draws on the student's own lane, never on the
   * tuition lane, so the amounts and dates above are the pre-round rows.
   *
   * EVERY ROW IS MADE, EVEN WITH NO SCHOOL. `make` routes device and IP on a
   * lane every family routine shares, and advances the payer's sticky device,
   * so skipping a row would move the sessions of every family row made after
   * it. A plan with no education record open throughout is made and dropped:
   * the rows vanish, the routing lane does not notice. */
  void emitPlan(entity::PersonId student, entity::Key payerAcct) {
    if (installments_.empty()) {
      return;
    }

    const auto first = installments_.front().timestamp;
    const auto last = installments_.back().timestamp;
    auto lane = schoolLane(run_.emission(), student);
    const auto school = schools_.pick(
        run_.education().homeAreaAt(student, first), first, last, lane);

    for (const auto &installment : installments_) {
      auto txn = run_.emission().make(transactions::Draft{
          .source = payerAcct,
          .destination = school.value_or(entity::Key{}),
          .amount = installment.amount,
          .timestamp = installment.timestamp,
          .isFraud = 0,
          .ringId = -1,
          .channel = channels::tag(channels::Family::tuition),
      });
      if (school.has_value()) {
        out_.push_back(std::move(txn));
      }
    }
  }

  const TransferRun &run_;
  const TuitionSchedule &cfg_;
  const schools::Directory &schools_;
  random::Rng &rng_;
  std::vector<transactions::Transaction> &out_;
  std::int64_t windowEndEpochSec_;
  std::vector<Installment> installments_;
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

  const schools::Directory schools{run.education().catalog()};
  if (schools.size() == 0) {
    return out;
  }

  // THE RETIRED PER-RUN PAYEE DRAW, SPENT VERBATIM (tuition-payee-2026-09).
  // Every plan paid the education record this one draw chose; each student
  // now picks on their own lane, but every coin, parent, amount and date
  // below is on this lane, so it still spends the draw it always did, over
  // the same range (every education record, open or not). Never size it from
  // the new law.
  (void)rng.uniformInt(0, static_cast<std::int64_t>(schools.size()));

  std::size_t studentCount = 0;
  for (entity::PersonId p = 1; p <= personCount; ++p) {
    if (pred::isStudent(run.kinship().persona(p))) {
      ++studentCount;
    }
  }
  out.reserve(estimateCapacity(studentCount, cfg));

  TuitionEmitter emitter{run, cfg, schools, rng, out};

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

    emitter.processStudent(student, std::span<const entity::PersonId>{
                                        parentBuf.data(), parentCount});
  }

  return out;
}

} // namespace PhantomLedger::transfers::legit::routines::family::tuition
