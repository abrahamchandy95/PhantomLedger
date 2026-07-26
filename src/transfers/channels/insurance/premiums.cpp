#include "phantomledger/transfers/channels/insurance/premiums.hpp"

#include "phantomledger/entities/products/insurance.hpp"
#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/utils/rounding.hpp"
#include "phantomledger/synth/econ/nominal.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/transactions/draft.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace PhantomLedger::transfers::insurance {

std::vector<transactions::Transaction>
PremiumGenerator::generate(const time::Window &window,
                           const entity::product::InsuranceLedger &insurance,
                           const entity::product::LoanTermsLedger &loans,
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

  // H3 part 3c-ii: premiums are CONTRACTUAL — the estate keeps paying
  // them until ACCOUNT CLOSURE (death + settlement). The skip sits
  // AFTER the hour/minute draws, so the shared rng stream is
  // byte-identical — only the post-closure rows disappear.
  const auto closeEpochOf = [&](PersonId person) -> std::int64_t {
    if (population.timelines.empty() || person == 0 ||
        static_cast<std::size_t>(person) > population.timelines.size()) {
      return std::numeric_limits<std::int64_t>::max();
    }
    return time::toEpochSeconds(population.timelines[person - 1].death) +
           static_cast<std::int64_t>(synth::pii::kSettlementDays) * 86'400;
  };

  // Post all monthly billing anchors for one policy from one payer.
  auto postPolicy = [&](const Key &payer, const InsurancePolicy &policy,
                        std::int64_t closeEpoch) {
    const int day = std::clamp(policy.billingDay, 1, 28);
    const auto anchors = almanac.monthly(window.start, endExcl, day);

    for (const auto base : anchors) {
      const auto ts = base + time::Hours{rng_->uniformInt(0, 6)} +
                      time::Minutes{rng_->uniformInt(0, 60)};

      if (time::toEpochSeconds(ts) >= closeEpoch) {
        continue; // the account closed — the draws above already burned
      }

      // H1 step 2b (class P): the policy's premium is drawn ONCE in
      // calibration-year dollars at world build; each billing realizes
      // it at the billing month's CPI level (authority U-6).
      out.push_back(txf_->make(transactions::Draft{
          .source = payer,
          .destination = policy.carrierAcct,
          .amount = primitives::utils::roundMoney(
              policy.monthlyPremium *
              synth::econ::priceScale(time::toCalendarDate(ts).year)),
          .timestamp = time::toEpochSeconds(ts),
          .channel = channel,
      }));
    }
  };

  insurance.forEach(
      [&](PersonId person, const entity::product::InsuranceHoldings &holdings) {
        const auto acctIt = population.primaryAccounts->find(person);
        if (acctIt == population.primaryAccounts->end()) {
          return;
        }

        const Key payer = acctIt->second;
        const auto closeEpoch = closeEpochOf(person);

        if (const auto &policy = holdings.autoPolicy(); policy.has_value()) {
          postPolicy(payer, *policy, closeEpoch);
        }

        if (const auto &policy = holdings.homePolicy();
            policy.has_value() && !loans.hasMortgage(person)) {
          postPolicy(payer, *policy, closeEpoch);
        }

        if (const auto &policy = holdings.lifePolicy(); policy.has_value()) {
          postPolicy(payer, *policy, closeEpoch);
        }
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});

  return out;
}

} // namespace PhantomLedger::transfers::insurance
