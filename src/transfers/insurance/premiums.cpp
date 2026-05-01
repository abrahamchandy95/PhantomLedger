#include "phantomledger/transfers/insurance/premiums.hpp"

#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <vector>

namespace PhantomLedger::transfers::insurance {
namespace {

using entity::Key;
using entity::PersonId;
using entity::product::InsurancePolicy;

void postPolicy(random::Rng &rng, const transactions::Factory &txf,
                time::Almanac &almanac, time::TimePoint start,
                time::TimePoint endExcl, const Key &payer,
                const InsurancePolicy &policy,
                std::vector<transactions::Transaction> &out) {
  const int day = std::clamp(policy.billingDay, 1, 28);
  const auto anchors = almanac.monthly(start, endExcl, day);
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

} // namespace

std::vector<transactions::Transaction>
premiums(const time::Window &window, random::Rng &rng,
         const transactions::Factory &txf,
         const entity::product::PortfolioRegistry &portfolios,
         const Population &population) {
  std::vector<transactions::Transaction> out;

  time::Almanac almanac{window};
  if (almanac.monthAnchors().empty()) {
    return out;
  }

  const auto endExcl = window.endExcl();

  portfolios.forEachInsuredPerson(
      [&](PersonId person, const entity::product::InsuranceHoldings &holdings) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end()) {
          return;
        }

        const Key payer = acctIt->second;

        if (const auto &policy = holdings.autoPolicy(); policy.has_value()) {
          postPolicy(rng, txf, almanac, window.start, endExcl, payer, *policy,
                     out);
        }

        if (const auto &policy = holdings.homePolicy();
            policy.has_value() && !portfolios.hasMortgage(person)) {
          postPolicy(rng, txf, almanac, window.start, endExcl, payer, *policy,
                     out);
        }

        if (const auto &policy = holdings.lifePolicy(); policy.has_value()) {
          postPolicy(rng, txf, almanac, window.start, endExcl, payer, *policy,
                     out);
        }
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});

  return out;
}

} // namespace PhantomLedger::transfers::insurance
