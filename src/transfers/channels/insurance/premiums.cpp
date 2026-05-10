#include "phantomledger/transfers/channels/insurance/premiums.hpp"

#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <vector>

namespace PhantomLedger::transfers::insurance {

std::vector<transactions::Transaction>
PremiumGenerator::generate(const time::Window &window,
                           const entity::product::PortfolioRegistry &portfolios,
                           const Population &population) const {
  using entity::Key;
  using entity::PersonId;
  using entity::product::InsurancePolicy;

  std::vector<transactions::Transaction> out;

  time::Almanac almanac{window};
  if (almanac.monthAnchors().empty()) {
    return out;
  }

  const auto endExcl = window.endExcl();
  const auto channel = channels::tag(channels::Insurance::premium);

  // Post all monthly billing anchors for one policy from one payer.
  auto postPolicy = [&](const Key &payer, const InsurancePolicy &policy) {
    const int day = std::clamp(policy.billingDay, 1, 28);
    const auto anchors = almanac.monthly(window.start, endExcl, day);

    for (const auto base : anchors) {
      const auto ts = base + time::Hours{rng_->uniformInt(0, 6)} +
                      time::Minutes{rng_->uniformInt(0, 60)};

      out.push_back(txf_->make(transactions::Draft{
          .source = payer,
          .destination = policy.carrierAcct,
          .amount = primitives::utils::roundMoney(policy.monthlyPremium),
          .timestamp = time::toEpochSeconds(ts),
          .channel = channel,
      }));
    }
  };

  portfolios.forEachInsuredPerson(
      [&](PersonId person, const entity::product::InsuranceHoldings &holdings) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end()) {
          return;
        }

        const Key payer = acctIt->second;

        if (const auto &policy = holdings.autoPolicy(); policy.has_value()) {
          postPolicy(payer, *policy);
        }

        if (const auto &policy = holdings.homePolicy();
            policy.has_value() && !portfolios.hasMortgage(person)) {
          postPolicy(payer, *policy);
        }

        if (const auto &policy = holdings.lifePolicy(); policy.has_value()) {
          postPolicy(payer, *policy);
        }
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});

  return out;
}

} // namespace PhantomLedger::transfers::insurance
