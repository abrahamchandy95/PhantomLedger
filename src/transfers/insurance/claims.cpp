#include "phantomledger/transfers/insurance/claims.hpp"

#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/probability/distributions/lognormal.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace PhantomLedger::transfers::insurance {
namespace {

using entity::Key;
using entity::PersonId;
using time::TimePoint;

[[nodiscard]] double windowClaimProbability(double annualP,
                                            std::size_t months) noexcept {
  if (annualP <= 0.0 || months == 0) {
    return 0.0;
  }
  const double years = static_cast<double>(months) / 12.0;
  return 1.0 - std::pow(1.0 - annualP, years);
}

[[nodiscard]] double sampleAmount(random::Rng &rng, double median, double sigma,
                                  double floor) {
  const auto raw =
      probability::distributions::lognormalByMedian(rng, median, sigma);
  return primitives::utils::floorAndRound(raw, floor);
}

void postClaim(random::Rng &rng, const transactions::Factory &txf,
               TimePoint start, TimePoint endExcl, int days, const Key &carrier,
               const Key &payer, double amount,
               std::vector<transactions::Transaction> &out) {
  if (amount <= 0.0) {
    return;
  }

  const auto dayOff = rng.uniformInt(0, std::max(1, days));
  const auto hour = rng.uniformInt(9, 17);
  const auto minute = rng.uniformInt(0, 60);
  const auto ts =
      start + time::Days{dayOff} + time::Hours{hour} + time::Minutes{minute};
  if (ts >= endExcl) {
    return;
  }

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
claims(const ClaimRates &rates, const time::Window &window, random::Rng &rng,
       const transactions::Factory &txf, const random::RngFactory &factory,
       const entity::product::PortfolioRegistry &portfolios,
       const Population &population) {
  std::vector<transactions::Transaction> out;

  time::Almanac almanac{window};
  const auto months = almanac.monthAnchors().size();
  if (months == 0) {
    return out;
  }

  const auto endExcl = window.endExcl();

  portfolios.forEachInsuredPerson(
      [&](PersonId person, const entity::product::InsuranceHoldings &h) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end()) {
          return;
        }
        const Key payer = acctIt->second;

        // Per-person sub-RNG isolates claim events from the main
        // stream so callers can re-run claims without perturbing
        // premiums or other generators.
        auto personRng =
            factory.rng({"insurance_claims",
                         std::to_string(static_cast<unsigned>(person))});

        if (h.auto_ && personRng.coin(windowClaimProbability(
                           h.auto_->annualClaimP, months))) {
          const double amt = sampleAmount(personRng, rates.autoMedian,
                                          rates.autoSigma, rates.autoFloor);
          postClaim(rng, txf, window.start, endExcl, window.days,
                    h.auto_->carrierAcct, payer, amt, out);
        }

        if (h.home && personRng.coin(windowClaimProbability(
                          h.home->annualClaimP, months))) {
          const double amt = sampleAmount(personRng, rates.homeMedian,
                                          rates.homeSigma, rates.homeFloor);
          postClaim(rng, txf, window.start, endExcl, window.days,
                    h.home->carrierAcct, payer, amt, out);
        }

        // Life insurance: death events are not modeled.
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfers::insurance
