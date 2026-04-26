#include "phantomledger/transfers/government/disability.hpp"

#include "phantomledger/primitives/time/almanac.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/government/recipients.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::government {

std::vector<transactions::Transaction>
disabilityBenefits(const DisabilityTerms &terms, const time::Window &window,
                   random::Rng &rng, const transactions::Factory &txf,
                   const Population &population,
                   const entity::Key &disabilityCounterparty) {
  std::vector<transactions::Transaction> out;

  time::Almanac almanac{window};
  if (almanac.monthAnchors().empty()) {
    return out;
  }

  auto recipients = select(population, rng, terms.eligibleP, terms.median,
                           terms.sigma, terms.floor, [](personas::Type t) {
                             return t != personas::Type::retiree &&
                                    t != personas::Type::student;
                           });

  monthlyDeposits(recipients, disabilityCounterparty,
                  channels::tag(channels::Government::disability), almanac,
                  window.start, window.endExcl(), rng, txf, out);

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfers::government
