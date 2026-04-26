#include "phantomledger/transfers/obligations/schedule.hpp"

#include "phantomledger/transfers/obligations/delinquency.hpp"
#include "phantomledger/transfers/obligations/installments.hpp"
#include "phantomledger/transfers/obligations/plain.hpp"

#include <algorithm>

namespace PhantomLedger::transfers::obligations {

std::vector<transactions::Transaction>
scheduledPayments(const entity::product::PortfolioRegistry &registry,
                  time::TimePoint start, time::TimePoint endExcl,
                  const Population &population, random::Rng &rng,
                  const transactions::Factory &txf) {
  std::vector<transactions::Transaction> out;
  delinquency::StateMap stateMap;

  for (const auto &event : registry.allEvents(start, endExcl)) {
    const auto acctIt = population.primaryAccounts->find(event.personId);
    if (acctIt == population.primaryAccounts->end()) {
      continue;
    }
    const entity::Key personAcct = acctIt->second;

    const bool routeThroughStateMachine =
        event.direction == entity::product::Direction::outflow &&
        entity::product::isInstallmentLoan(event.productType);

    if (routeThroughStateMachine &&
        registry.installmentTerms(event.personId, event.productType) !=
            nullptr) {
      installments::postEvent(out, registry, stateMap, rng, txf, event,
                              personAcct,

                              endExcl);
    } else {
      plain::postEvent(out, rng, txf, event, personAcct, endExcl);
    }
  }

  std::sort(out.begin(), out.end(),
            [](const transactions::Transaction &a,
               const transactions::Transaction &b) noexcept {
              return a.timestamp < b.timestamp;
            });

  return out;
}

} // namespace PhantomLedger::transfers::obligations
