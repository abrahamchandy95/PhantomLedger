#include "phantomledger/transfers/insurance/claims.hpp"

#include "phantomledger/entities/products/insurance.hpp"
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
#include <vector>

namespace PhantomLedger::transfers::insurance {
namespace {

using entity::Key;
using entity::PersonId;
using time::TimePoint;

namespace product = entity::product;

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

void tryPostClaim(random::Rng &claimRng, random::Rng &timestampRng,
                  const transactions::Factory &txf,
                  const product::InsuranceHoldings &holdings,
                  product::PolicyType policyType, TimePoint start,
                  TimePoint endExcl, int days, const Key &payer, double median,
                  double sigma, double floor, std::size_t months,
                  std::vector<transactions::Transaction> &out) {
  const auto *policy = holdings.get(policyType);
  if (policy == nullptr) {
    return;
  }

  const double claimP = windowClaimProbability(policy->annualClaimP, months);
  if (!claimRng.coin(claimP)) {
    return;
  }

  const double amount = sampleAmount(claimRng, median, sigma, floor);

  postClaim(timestampRng, txf, start, endExcl, days, policy->carrierAcct, payer,
            amount, out);
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
      [&](PersonId person, const product::InsuranceHoldings &holdings) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end()) {
          return;
        }

        const Key payer = acctIt->second;

        // Per-person sub-RNG isolates claim occurrence and amount sampling.
        auto personRng =
            factory.rng({"insurance_claims",
                         std::to_string(static_cast<unsigned>(person))});

        tryPostClaim(personRng, rng, txf, holdings, product::PolicyType::auto_,
                     window.start, endExcl, window.days, payer,
                     rates.autoMedian, rates.autoSigma, rates.autoFloor, months,
                     out);

        tryPostClaim(personRng, rng, txf, holdings, product::PolicyType::home,
                     window.start, endExcl, window.days, payer,
                     rates.homeMedian, rates.homeSigma, rates.homeFloor, months,
                     out);

        // Life insurance: death events are not modeled.
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});

  return out;
}

} // namespace PhantomLedger::transfers::insurance
