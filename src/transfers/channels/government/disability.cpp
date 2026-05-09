#include "phantomledger/transfers/channels/government/disability.hpp"

#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/channels/government/monthly_deposit_emitter.hpp"
#include "phantomledger/transfers/channels/government/recipients.hpp"

#include <algorithm>
#include <span>

namespace PhantomLedger::transfers::government {

std::vector<transactions::Transaction>
disabilityBenefits(const DisabilityTerms &terms, const time::Window &window,
                   random::Rng &rng, const transactions::Factory &txf,
                   const Population &population,
                   const entity::Key &disabilityCounterparty) {
  MonthlyDepositEmitter deposits{window, rng, txf};
  if (!deposits.active()) {
    return {};
  }

  auto recipients = select(population, rng, terms, [](personas::Type t) {
    return t != personas::Type::retiree && t != personas::Type::student;
  });

  auto out = deposits.emit(
      std::span<const Recipient>{recipients},
      BenefitDeposit{
          .source = disabilityCounterparty,
          .channel = channels::tag(channels::Government::disability),
      });

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfers::government
