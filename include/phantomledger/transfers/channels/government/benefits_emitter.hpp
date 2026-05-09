#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/government/monthly_deposit_emitter.hpp"
#include "phantomledger/transfers/channels/government/recipients.hpp"

#include <algorithm>
#include <span>
#include <vector>

namespace PhantomLedger::transfers::government {

struct BenefitProgram {
  using PersonaPredicate = bool (*)(personas::Type);
  PersonaPredicate eligible = nullptr;
  entity::Key source{};
  channels::Tag channel{};
};

class BenefitsEmitter {
public:
  BenefitsEmitter(const time::Window &window, random::Rng &rng,
                  const transactions::Factory &txf,
                  const Population &population) noexcept
      : window_(window), rng_(rng), txf_(txf), population_(population) {}

  template <class Terms>
  [[nodiscard]] std::vector<transactions::Transaction>
  emit(const Terms &terms, const BenefitProgram &program);

private:
  time::Window window_{};
  random::Rng &rng_;
  const transactions::Factory &txf_;
  const Population &population_;
};

template <class Terms>
inline std::vector<transactions::Transaction>
BenefitsEmitter::emit(const Terms &terms, const BenefitProgram &program) {
  MonthlyDepositEmitter deposits{window_, rng_, txf_};
  if (!deposits.active()) {
    return {};
  }

  auto recipients = select(population_, rng_, terms, program.eligible);

  auto out = deposits.emit(
      std::span<const Recipient>{recipients},
      BenefitDeposit{.source = program.source, .channel = program.channel});

  std::sort(
      out.begin(), out.end(),
      transactions::Comparator{transactions::Comparator::Scope::fundsTransfer});
  return out;
}

} // namespace PhantomLedger::transfers::government
