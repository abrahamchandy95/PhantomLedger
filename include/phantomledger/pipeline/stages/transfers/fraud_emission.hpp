#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/transfers/fraud/injector_views.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

namespace PhantomLedger::transfers::fraud {

struct Behavior;
} // namespace PhantomLedger::transfers::fraud

namespace PhantomLedger::pipeline::stages::transfers {

class FraudEmission {
public:
  FraudEmission() = default;

  FraudEmission &profile(
      const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept;

  FraudEmission &
  behavior(const ::PhantomLedger::transfers::fraud::Behavior *value) noexcept;

  [[nodiscard]] const ::PhantomLedger::transfers::fraud::Behavior &
  resolvedBehavior() const noexcept;

  [[nodiscard]] ::PhantomLedger::transfers::fraud::InjectorRingView ringView(
      const ::PhantomLedger::entity::person::Topology &topology) const noexcept;

  [[nodiscard]] static ::PhantomLedger::transfers::fraud::InjectorAccountView
  accountView(
      const ::PhantomLedger::entity::account::Registry &registry,
      const ::PhantomLedger::entity::account::Ownership &ownership) noexcept;

  [[nodiscard]] static ::PhantomLedger::transfers::fraud::
      InjectorLegitCounterparties
      legitCounterparties(
          const ::PhantomLedger::transfers::legit::ledger::LegitCounterparties
              &counterparties) noexcept;

private:
  const ::PhantomLedger::entities::synth::people::Fraud *profile_ = nullptr;
  const ::PhantomLedger::transfers::fraud::Behavior *behavior_ = nullptr;
};

} // namespace PhantomLedger::pipeline::stages::transfers
