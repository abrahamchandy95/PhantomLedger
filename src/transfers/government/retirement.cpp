#include "phantomledger/transfers/government/retirement.hpp"

#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/government/recipients.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::government {

std::vector<transactions::Transaction>
retirementBenefits(const RetirementTerms &terms, const time::Window &window,
                   random::Rng &rng, const transactions::Factory &txf,
                   const Population &population,
                   const entity::Key &ssaCounterparty) {
  std::vector<transactions::Transaction> out;

  time::Almanac almanac{window};
  if (almanac.monthAnchors().empty()) {
    return out;
  }

  auto recipients = select(
      population, rng, terms.eligibleP, terms.median, terms.sigma, terms.floor,
      [](personas::Type t) { return t == personas::Type::retiree; });

  monthlyDeposits(recipients, ssaCounterparty,
                  channels::tag(channels::Government::socialSecurity), almanac,
                  window.start, window.endExcl(), rng, txf, out);

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfers::government
