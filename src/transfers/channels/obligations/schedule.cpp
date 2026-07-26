#include "phantomledger/transfers/channels/obligations/schedule.hpp"

#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/transfers/channels/obligations/installments.hpp"
#include "phantomledger/transfers/channels/obligations/plain.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
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

// H3 part 3c-ii: loan/tax obligations are CONTRACTUAL — the estate
// services them until ACCOUNT CLOSURE (death + settlement).
[[nodiscard]] std::int64_t closeEpochOf(const Population &population,
                                        entity::PersonId person) {
  if (population.timelines.empty() || person == 0 ||
      static_cast<std::size_t>(person) > population.timelines.size()) {
    return std::numeric_limits<std::int64_t>::max();
  }
  return time::toEpochSeconds(population.timelines[person - 1].death) +
         static_cast<std::int64_t>(synth::pii::kSettlementDays) * 86'400;
}

void appendDraft(std::vector<transactions::Transaction> &out,
                 const transactions::Factory &txf,
                 const std::optional<transactions::Draft> &draft,
                 std::int64_t closeEpoch) {
  if (!draft.has_value()) {
    return;
  }

  // The skip sits AFTER draftFor's internal draws burned, so the
  // shared rng stream is byte-identical — only the post-closure rows
  // (and their downstream screen postings) disappear.
  if (draft->timestamp >= closeEpoch) {
    return;
  }

  out.push_back(txf.make(*draft));
}

} // namespace

Scheduler::Scheduler(random::Rng &rng,
                     const transactions::Factory &txf) noexcept
    : rng_(&rng), txf_(&txf) {}

std::vector<transactions::Transaction>
Scheduler::generate(const entity::product::LoanTermsLedger &loans,
                    const entity::product::ObligationStream &obligations,
                    time::HalfOpenInterval active,
                    const Population &population) const {
  std::vector<transactions::Transaction> out;
  installments::EventEmitter installmentEvents{loans};

  for (const auto &event : obligations.between(active.start, active.endExcl)) {
    const auto personAcct = primaryAccount(population, event.personId);
    if (!personAcct.has_value()) {
      continue;
    }

    const auto closeEpoch = closeEpochOf(population, event.personId);

    if (installments::tracks(loans, event)) {
      appendDraft(out, *txf_,
                  installmentEvents.draftFor(*rng_, event, *personAcct,
                                             active.endExcl),
                  closeEpoch);
    } else {
      appendDraft(out, *txf_,
                  plain::draftFor(*rng_, event, *personAcct, active.endExcl),
                  closeEpoch);
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
