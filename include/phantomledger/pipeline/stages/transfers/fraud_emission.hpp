#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

namespace PhantomLedger::pipeline::stages::transfers {

class FraudEmission {
public:
  struct Programs {
    const ::PhantomLedger::entities::synth::people::Fraud *profile = nullptr;
    ::PhantomLedger::transfers::fraud::Injector::Patterns patterns{};
  };

  FraudEmission() = default;

  FraudEmission &programs(Programs value) noexcept;
  FraudEmission &profile(
      const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept;
  FraudEmission &
  rules(::PhantomLedger::transfers::fraud::Injector::Patterns value) noexcept;

  [[nodiscard]] ::PhantomLedger::transfers::fraud::Injector
  makeInjector(::PhantomLedger::transfers::fraud::Injector::Services services,
               ::PhantomLedger::transfers::fraud::Injector::RingView rings,
               ::PhantomLedger::transfers::fraud::Injector::AccountView
                   accounts) const noexcept;

  [[nodiscard]] ::PhantomLedger::transfers::fraud::Injector::RingView ringView(
      const ::PhantomLedger::entity::person::Topology &topology) const noexcept;

  [[nodiscard]] static ::PhantomLedger::transfers::fraud::Injector::AccountView
  accountView(
      const ::PhantomLedger::entity::account::Registry &registry,
      const ::PhantomLedger::entity::account::Ownership &ownership) noexcept;

  [[nodiscard]] static ::PhantomLedger::transfers::fraud::Injector::
      LegitCounterparties
      legitCounterparties(
          const ::PhantomLedger::transfers::legit::ledger::LegitCounterparties
              &counterparties) noexcept;

private:
  Programs programs_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
