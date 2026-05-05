#include "phantomledger/transfers/obligations/schedule.hpp"

#include "phantomledger/transfers/obligations/installments.hpp"
#include "phantomledger/transfers/obligations/plain.hpp"

#include <algorithm>
#include <optional>

namespace PhantomLedger::transfers::obligations {
namespace {

[[nodiscard]] std::optional<entity::Key>
primaryAccount(const Population &population, entity::PersonId person) {
  const auto acctIt = population.primaryAccounts->find(person);
  if (acctIt == population.primaryAccounts->end()) {
    return std::nullopt;
  }
  return acctIt->second;
}

void appendDraft(std::vector<transactions::Transaction> &out,
                 const transactions::Factory &txf,
                 const std::optional<transactions::Draft> &draft) {
  if (!draft.has_value()) {
    return;
  }

  out.push_back(txf.make(*draft));
}

} // namespace

Scheduler::Scheduler(random::Rng &rng,
                     const transactions::Factory &txf) noexcept
    : rng_(&rng), txf_(&txf) {}

std::vector<transactions::Transaction>
Scheduler::generate(const entity::product::PortfolioRegistry &registry,
                    time::HalfOpenInterval active,
                    const Population &population) const {
  std::vector<transactions::Transaction> out;
  installments::EventEmitter installmentEvents{registry};

  for (const auto &event : registry.allEvents(active.start, active.endExcl)) {
    const auto personAcct = primaryAccount(population, event.personId);
    if (!personAcct.has_value()) {
      continue;
    }

    if (installments::tracks(registry, event)) {
      appendDraft(out, *txf_,
                  installmentEvents.draftFor(*rng_, event, *personAcct,
                                             active.endExcl));
    } else {
      appendDraft(out, *txf_,
                  plain::draftFor(*rng_, event, *personAcct, active.endExcl));
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
