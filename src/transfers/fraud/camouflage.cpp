#include "phantomledger/transfers/fraud/camouflage.hpp"

#include "phantomledger/activity/income/timestamps.hpp"
#include "phantomledger/activity/recurring/employment.hpp"
#include "phantomledger/activity/recurring/growth.hpp"
#include "phantomledger/activity/recurring/payroll.hpp"
#include "phantomledger/math/amounts.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::transfers::fraud::camouflage {

namespace {

namespace recur = activity::recurring;
namespace timestamps = activity::income::timestamps;

// H1 step 2b (authority U-6): camouflage traffic scales with the index
// of the flow it MIMICS — bills/p2p ride the CPI (class P), the salary
// mimic rides the AWI (class W). A cover row scaled differently from
// its cover class would be a detectable artifact.
[[nodiscard]] double nominalPrice(double amount, time::TimePoint ts) {
  return primitives::utils::roundMoney(
      amount * synth::econ::priceScale(time::toCalendarDate(ts).year));
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
  const auto startDate = ctx.window.start;
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
            .amount = nominalPrice(math::amounts::kBill.sample(rng), ts),
            .timestamp = time::toEpochSeconds(ts),
            .isFraud = 0,
            .ringId = -1,
            .channel = billChannel,
        }));
      }
    }
  }

  // ---- 2. Small daily P2P --------------------------------------------
  if (!ctx.accounts->depositAccounts.empty() && rates.smallP2pPerDayP > 0.0) {
    for (std::int32_t day = 0; day < days; ++day) {
      const auto dayStart = startDate + time::Days{day};

      for (const auto &acct : ringAccounts) {
        if (!rng.coin(rates.smallP2pPerDayP)) {
          continue;
        }

        // The pool holds only customer deposit accounts, the destinations
        // legitimate P2P pays (fraud::camouflageEligible), so one pick is
        // final. The filter replaced a bounded re-pick of merchant
        // destinations (merchant-churn-2026-07) and, like it, removes a
        // destination rather than the row: camouflage VOLUME is the whole
        // point of camouflage. Only a pick of the sender's own account skips.
        const entity::Key dst = ctx.accounts->depositAccounts[rng.choiceIndex(
            ctx.accounts->depositAccounts.size())];

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
            .amount = nominalPrice(math::amounts::kP2P.sample(rng), ts),
            .timestamp = time::toEpochSeconds(ts),
            .isFraud = 0,
            .ringId = -1,
            .channel = p2pChannel,
        }));
      }
    }
  }

  // ---- 3. Recurring inbound salary -----------------------------------
  if (ctx.accounts->employers != nullptr && !ctx.accounts->employers->empty() &&
      ctx.payrollFactory != nullptr && rates.salaryInboundP > 0.0) {
    for (const auto &acct : ringAccounts) {
      if (!rng.coin(rates.salaryInboundP)) {
        continue;
      }

      // The payroll size law, not a uniform pick (counterparty-sizes-2026-09):
      // a uniform pick over a roster that is mostly one-payee firms would put
      // almost every mule salary on an employer nobody else is paid by. Same
      // single u64 on the camo lane as the choiceIndex it replaced.
      const auto &src = recur::growth::pickSized(rng, *ctx.accounts->employers);

      // The picked employer's OWN schedule, derived from the lane legitimate
      // payroll reads ({employer_payroll_profile, number} on the run seed's
      // factory, under the default rules it also runs on) and posted the way
      // its rows are: the posting lag, no day jitter, 06:00 to 11:59. A
      // schedule drawn here instead put 72% of the mule salary streams from
      // an employer with legitimate payees on at least one date that
      // employer pays nobody else. The derivation spends nothing on the camo
      // lane, where the independent schedule spent three or four draws.
      const auto profile = recur::samplePayrollProfile(
          recur::PayrollRules{}, *ctx.payrollFactory, src);
      const double annualSalary = math::amounts::kSalary.sample(rng) * 12.0;

      const auto payDates =
          recur::paydatesForProfile(profile, startDate, windowEndExcl);

      for (const auto &payDate : payDates) {
        const auto ts =
            timestamps::jittered(payDate, profile.postingLagDays,
                                 timestamps::kSalaryTimestampJitter, rng);

        if (ts < startDate || ts >= windowEndExcl) {
          continue;
        }

        const auto payDateCal = time::toCalendarDate(payDate);
        const int periods = recur::payPeriodsInYear(profile, payDateCal.year);

        // Salary mimic: floored calibration paycheck × the pay-date
        // year's WAGE index (mirrors SalaryCalculator's shape).
        const double rawAmount = annualSalary / static_cast<double>(periods);
        const double amount = primitives::utils::roundMoney(
            primitives::utils::floorAndRound(rawAmount, 50.0) *
            synth::econ::wageScale(payDateCal.year));

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
