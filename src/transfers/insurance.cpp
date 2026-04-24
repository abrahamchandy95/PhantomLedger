#include "phantomledger/transfers/insurance.hpp"

#include "phantomledger/primitives/time/window_calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace PhantomLedger::transfer::insurance {
namespace {

using Key = entity::Key;
using PersonId = entity::PersonId;
using TimePoint = time::TimePoint;
using entity::product::InsurancePolicy;

/// Annual → window probability using the complement of survival.
/// Independent across months; exact for a memoryless claim process.
[[nodiscard]] double windowClaimProbability(double annualP,
                                            std::size_t months) noexcept {
  if (annualP <= 0.0 || months == 0)
    return 0.0;
  const double years = static_cast<double>(months) / 12.0;
  return 1.0 - std::pow(1.0 - annualP, years);
}

[[nodiscard]] double sampleClaim(random::Rng &rng, double median, double sigma,
                                 double floor) {
  const auto raw =
      probability::distributions::lognormalByMedian(rng, median, sigma);
  return primitives::utils::roundMoney(std::max(floor, raw));
}

void emitPremiums(random::Rng &rng, const transactions::Factory &txf,
                  time::WindowCalendar &calendar, TimePoint start,
                  TimePoint endExcl, const Key &payer,
                  const InsurancePolicy &policy,
                  std::vector<transactions::Transaction> &out) {
  const int day = std::clamp(policy.billingDay, 1, 28);
  const auto anchors = calendar.iterMonthly(start, endExcl, day);
  const auto channel = channels::tag(channels::Insurance::premium);

  for (const auto base : anchors) {
    const auto ts = base + time::Hours{rng.uniformInt(0, 6)} +
                    time::Minutes{rng.uniformInt(0, 60)};

    out.push_back(txf.make(transactions::Draft{
        .source = payer,
        .destination = policy.carrierAcct,
        .amount = primitives::utils::roundMoney(policy.monthlyPremium),
        .timestamp = time::toEpochSeconds(ts),
        .channel = channel,
    }));
  }
}

void emitClaim(random::Rng &rng, const transactions::Factory &txf,
               TimePoint start, TimePoint endExcl, int days, const Key &carrier,
               const Key &payer, double amount,
               std::vector<transactions::Transaction> &out) {
  if (amount <= 0.0)
    return;

  const auto dayOff = rng.uniformInt(0, std::max(1, days));
  const auto hour = rng.uniformInt(9, 17);
  const auto minute = rng.uniformInt(0, 60);
  const auto ts =
      start + time::Days{dayOff} + time::Hours{hour} + time::Minutes{minute};
  if (ts >= endExcl)
    return;

  out.push_back(txf.make(transactions::Draft{
      .source = carrier,
      .destination = payer,
      .amount = amount,
      .timestamp = time::toEpochSeconds(ts),
      .channel = channels::tag(channels::Insurance::claim),
  }));
}

} // namespace

std::vector<transactions::Transaction>
generate(const Params &params, const Window &window, random::Rng &rng,
         const transactions::Factory &txf, const random::RngFactory &factory,
         const entity::product::PortfolioRegistry &portfolios,
         const Population &population) {
  std::vector<transactions::Transaction> out;

  const auto endExcl = window.endExcl();
  time::WindowCalendar calendar{window.start, endExcl};
  const auto months = calendar.monthAnchors().size();
  if (months == 0)
    return out;

  portfolios.forEachInsuredPerson(
      [&](PersonId person, const entity::product::InsuranceHoldings &h) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end())
          return;
        const Key payer = acctIt->second;

        auto personRng = factory.rng(
            {"insurance", std::to_string(static_cast<unsigned>(person))});

        if (h.auto_) {
          emitPremiums(rng, txf, calendar, window.start, endExcl, payer,
                       *h.auto_, out);
          if (personRng.coin(
                  windowClaimProbability(h.auto_->annualClaimP, months))) {
            const double amt = sampleClaim(personRng, params.autoClaimMedian,
                                           params.autoClaimSigma, 500.0);
            emitClaim(rng, txf, window.start, endExcl, window.days,
                      h.auto_->carrierAcct, payer, amt, out);
          }
        }

        if (h.home) {
          // Mortgage escrow covers home premium; claims still fire.
          if (!portfolios.hasMortgage(person)) {
            emitPremiums(rng, txf, calendar, window.start, endExcl, payer,
                         *h.home, out);
          }
          if (personRng.coin(
                  windowClaimProbability(h.home->annualClaimP, months))) {
            const double amt = sampleClaim(personRng, params.homeClaimMedian,
                                           params.homeClaimSigma, 1000.0);
            emitClaim(rng, txf, window.start, endExcl, window.days,
                      h.home->carrierAcct, payer, amt, out);
          }
        }

        if (h.life) {
          emitPremiums(rng, txf, calendar, window.start, endExcl, payer,
                       *h.life, out);
        }
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfer::insurance
