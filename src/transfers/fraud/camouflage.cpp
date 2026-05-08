#include "phantomledger/transfers/fraud/camouflage.hpp"

#include "phantomledger/activity/recurring/payroll.hpp"
#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/probability/distributions/cdf.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::camouflage {

namespace {

using recurring::PayCadence;
using recurring::PayrollProfile;
using recurring::PayrollRules;

[[nodiscard]] PayrollProfile sampleCamouflagePayrollProfile(random::Rng &rng) {
  static constexpr PayrollRules kRules{};

  const std::array<double, recurring::kPayCadenceCount> cadenceWeights{
      kRules.weights.weekly,
      kRules.weights.biweekly,
      kRules.weights.semimonthly,
      kRules.weights.monthly,
  };

  const auto cdf = distributions::buildCdf(cadenceWeights);
  const auto cadence = recurring::kPayCadences[distributions::sampleIndex(
      cdf, rng.nextDouble())];

  int weekday = kRules.defaultWeekday;
  if ((cadence == PayCadence::weekly || cadence == PayCadence::biweekly) &&
      rng.coin(0.25)) {
    weekday = (weekday == 4) ? 3 : 4;
  }

  auto anchor =
      recurring::nextWeekdayOnOrAfter(time::makeTime(time::CalendarDate{
                                          .year = 2025,
                                          .month = 1,
                                          .day = 1,
                                      }),
                                      weekday);

  if (cadence == PayCadence::biweekly && rng.coin(0.5)) {
    anchor = time::addDays(anchor, 7);
  }

  const int lagMax = kRules.postingLagDaysMax;
  const int postingLagDays =
      (lagMax == 0) ? 0
                    : static_cast<int>(rng.uniformInt(
                          0, static_cast<std::int64_t>(lagMax) + 1));

  PayrollProfile profile;
  profile.cadence = cadence;
  profile.anchorDate = anchor;
  profile.weekday = weekday;
  profile.postingLagDays = postingLagDays;

  if (cadence == PayCadence::semimonthly) {
    profile.semimonthlyDays =
        rng.coin(0.35) ? std::array<int, 2>{1, 15} : std::array<int, 2>{15, 31};
  }

  if (cadence == PayCadence::monthly) {
    static constexpr std::array<int, 3> kMonthlyDayChoices{28, 30, 31};
    profile.monthlyDay =
        kMonthlyDayChoices[rng.choiceIndex(kMonthlyDayChoices.size())];
  }

  primitives::validate::require(profile);
  return profile;
}

} // namespace

std::vector<transactions::Transaction>
generate(CamouflageContext &ctx, const Plan &plan, const Rates &rates) {
  const auto ringAccounts = plan.participantAccounts();
  if (ringAccounts.empty() || ctx.accounts == nullptr) {
    return {};
  }

  std::vector<transactions::Transaction> out;

  random::Rng &rng = *ctx.execution.rng;
  const auto startDate = ctx.window.startDate;
  const auto windowEndExcl = ctx.window.endExcl();
  const auto days = ctx.window.days;

  const auto billChannel = channels::tag(channels::Camouflage::bill);
  const auto p2pChannel = channels::tag(channels::Camouflage::p2p);
  const auto salaryChannel = channels::tag(channels::Camouflage::salary);

  // ---- 1. Monthly bills ----------------------------------------------
  if (!ctx.accounts->billerAccounts.empty() && rates.billMonthlyP > 0.0) {
    const auto monthAnchors = time::monthStarts(startDate, windowEndExcl);

    for (const auto &payDay : monthAnchors) {
      for (const auto &acct : ringAccounts) {
        if (!rng.coin(rates.billMonthlyP)) {
          continue;
        }

        const auto &dst = ctx.accounts->billerAccounts[rng.choiceIndex(
            ctx.accounts->billerAccounts.size())];

        const auto offsetDays = static_cast<std::int32_t>(rng.uniformInt(0, 6));
        const auto offsetHours =
            static_cast<std::int32_t>(rng.uniformInt(7, 23));
        const auto offsetMinutes =
            static_cast<std::int32_t>(rng.uniformInt(0, 61));

        const auto ts = payDay + time::Days{offsetDays} +
                        time::Hours{offsetHours} + time::Minutes{offsetMinutes};

        if (ts >= windowEndExcl) {
          continue;
        }

        out.push_back(ctx.execution.txf.make(transactions::Draft{
            .source = acct,
            .destination = dst,
            .amount = math::amounts::kBill.sample(rng),
            .timestamp = time::toEpochSeconds(ts),
            .isFraud = 0,
            .ringId = -1,
            .channel = billChannel,
        }));
      }
    }
  }

  // ---- 2. Small daily P2P --------------------------------------------
  if (!ctx.accounts->allAccounts.empty() && rates.smallP2pPerDayP > 0.0) {
    for (std::int32_t day = 0; day < days; ++day) {
      const auto dayStart = startDate + time::Days{day};

      for (const auto &acct : ringAccounts) {
        if (!rng.coin(rates.smallP2pPerDayP)) {
          continue;
        }

        const auto &dst = ctx.accounts->allAccounts[rng.choiceIndex(
            ctx.accounts->allAccounts.size())];

        if (dst == acct) {
          continue;
        }

        const auto offsetHours =
            static_cast<std::int32_t>(rng.uniformInt(0, 25));
        const auto offsetMinutes =
            static_cast<std::int32_t>(rng.uniformInt(0, 61));

        const auto ts =
            dayStart + time::Hours{offsetHours} + time::Minutes{offsetMinutes};

        out.push_back(ctx.execution.txf.make(transactions::Draft{
            .source = acct,
            .destination = dst,
            .amount = math::amounts::kP2P.sample(rng),
            .timestamp = time::toEpochSeconds(ts),
            .isFraud = 0,
            .ringId = -1,
            .channel = p2pChannel,
        }));
      }
    }
  }

  // ---- 3. Recurring inbound salary -----------------------------------
  if (!ctx.accounts->employers.empty() && rates.salaryInboundP > 0.0) {
    for (const auto &acct : ringAccounts) {
      if (!rng.coin(rates.salaryInboundP)) {
        continue;
      }

      const auto &src =
          ctx.accounts
              ->employers[rng.choiceIndex(ctx.accounts->employers.size())];

      const auto profile = sampleCamouflagePayrollProfile(rng);
      const double annualSalary = math::amounts::kSalary.sample(rng) * 12.0;

      const auto payDates =
          recurring::paydatesForProfile(profile, startDate, windowEndExcl);

      for (const auto &payDate : payDates) {
        const auto offsetHours =
            static_cast<std::int32_t>(rng.uniformInt(6, 12));
        const auto offsetMinutes =
            static_cast<std::int32_t>(rng.uniformInt(0, 61));

        const auto ts =
            payDate + time::Hours{offsetHours} + time::Minutes{offsetMinutes};

        if (ts < startDate || ts >= windowEndExcl) {
          continue;
        }

        const auto payDateCal = time::toCalendarDate(payDate);
        const int periods =
            recurring::payPeriodsInYear(profile, payDateCal.year);

        const double rawAmount = annualSalary / static_cast<double>(periods);
        const double amount =
            std::round(std::max(50.0, rawAmount) * 100.0) / 100.0;

        out.push_back(ctx.execution.txf.make(transactions::Draft{
            .source = src,
            .destination = acct,
            .amount = amount,
            .timestamp = time::toEpochSeconds(ts),
            .isFraud = 0,
            .ringId = -1,
            .channel = salaryChannel,
        }));
      }
    }
  }

  return out;
}

} // namespace PhantomLedger::transfers::fraud::camouflage
