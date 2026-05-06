#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/transfers/fraud/camouflage.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/fraud/typologies/layering.hpp"
#include "phantomledger/transfers/fraud/typologies/structuring.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

namespace PhantomLedger::pipeline::stages::transfers {

class FraudEmission {
public:
  FraudEmission() = default;

  FraudEmission &profile(
      const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept;
  FraudEmission &typologyWeights(
      ::PhantomLedger::transfers::fraud::TypologyWeights value) noexcept;
  FraudEmission &
  layeringRules(::PhantomLedger::transfers::fraud::typologies::layering::Rules
                    value) noexcept;
  FraudEmission &structuringRules(
      ::PhantomLedger::transfers::fraud::typologies::structuring::Rules
          value) noexcept;
  FraudEmission &camouflageRates(
      ::PhantomLedger::transfers::fraud::camouflage::Rates value) noexcept;

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
  const ::PhantomLedger::entities::synth::people::Fraud *profile_ = nullptr;
  ::PhantomLedger::transfers::fraud::TypologyWeights typologyWeights_{};
  ::PhantomLedger::transfers::fraud::typologies::layering::Rules
      layeringRules_{};
  ::PhantomLedger::transfers::fraud::typologies::structuring::Rules
      structuringRules_{};
  ::PhantomLedger::transfers::fraud::camouflage::Rates camouflageRates_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
