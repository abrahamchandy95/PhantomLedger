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

class PremiumEmitter {
public:
  PremiumEmitter(random::Rng &rng, const transactions::Factory &txf,
                 time::Almanac &almanac, time::TimePoint start,
                 time::TimePoint endExcl,
                 std::vector<transactions::Transaction> &out) noexcept
      : rng_(rng), txf_(txf), almanac_(almanac), start_(start),
        endExcl_(endExcl), out_(out) {}

  PremiumEmitter(const PremiumEmitter &) = delete;
  PremiumEmitter &operator=(const PremiumEmitter &) = delete;

  /// Post all monthly billing anchors for one policy from one payer.
  void postPolicy(const Key &payer, const InsurancePolicy &policy) {
    const int day = std::clamp(policy.billingDay, 1, 28);
    const auto anchors = almanac_.monthly(start_, endExcl_, day);
    const auto channel = channels::tag(channels::Insurance::premium);

    for (const auto base : anchors) {
      const auto ts = base + time::Hours{rng_.uniformInt(0, 6)} +
                      time::Minutes{rng_.uniformInt(0, 60)};

      out_.push_back(txf_.make(transactions::Draft{
          .source = payer,
          .destination = policy.carrierAcct,
          .amount = primitives::utils::roundMoney(policy.monthlyPremium),
          .timestamp = time::toEpochSeconds(ts),
          .channel = channel,
      }));
    }
  }

private:
  random::Rng &rng_;
  const transactions::Factory &txf_;
  time::Almanac &almanac_;
  time::TimePoint start_;
  time::TimePoint endExcl_;
  std::vector<transactions::Transaction> &out_;
};

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

  PremiumEmitter emitter{rng, txf, almanac, window.start, endExcl, out};

  portfolios.forEachInsuredPerson(
      [&](PersonId person, const entity::product::InsuranceHoldings &holdings) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end()) {
          return;
        }

        const Key payer = acctIt->second;

        if (const auto &policy = holdings.autoPolicy(); policy.has_value()) {
          emitter.postPolicy(payer, *policy);
        }

        if (const auto &policy = holdings.homePolicy();
            policy.has_value() && !portfolios.hasMortgage(person)) {
          emitter.postPolicy(payer, *policy);
        }

        if (const auto &policy = holdings.lifePolicy(); policy.has_value()) {
          emitter.postPolicy(payer, *policy);
        }
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});

  return out;
}

} // namespace PhantomLedger::transfers::insurance
