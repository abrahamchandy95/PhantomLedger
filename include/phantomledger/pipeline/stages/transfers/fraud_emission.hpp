#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <span>

namespace PhantomLedger::infra {
class Router;
struct SharedInfra;
} // namespace PhantomLedger::infra

namespace PhantomLedger::pipeline::stages::transfers {

class FraudEmission {
public:
  using Transaction = ::PhantomLedger::transactions::Transaction;

  struct Programs {
    const ::PhantomLedger::entities::synth::people::Fraud *profile = nullptr;
    ::PhantomLedger::transfers::fraud::Injector::Rules injectorRules{};
  };

  FraudEmission() = default;

  FraudEmission &programs(Programs value) noexcept;
  FraudEmission &profile(
      const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept;
  FraudEmission &
  rules(::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept;

  [[nodiscard]] ::PhantomLedger::transfers::fraud::InjectionOutput
  inject(::PhantomLedger::random::Rng &rng,
         const ::PhantomLedger::infra::Router &router,
         const ::PhantomLedger::infra::SharedInfra &ringInfra,
         ::PhantomLedger::time::Window window,
         const ::PhantomLedger::entity::person::Topology &topology,
         const ::PhantomLedger::entity::account::Registry &accounts,
         const ::PhantomLedger::entity::account::Ownership &ownership,
         std::span<const Transaction> draftTxns,
         const ::PhantomLedger::transfers::legit::ledger::TransfersPayload
             &legitPayload) const;

private:
  Programs programs_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
