#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"

#include "phantomledger/transfers/fraud/behavior.hpp"

namespace PhantomLedger::pipeline::stages::transfers {

namespace fraud = ::PhantomLedger::transfers::fraud;

FraudEmission &FraudEmission::profile(
    const ::PhantomLedger::synth::people::Fraud *value) noexcept {
  profile_ = value;
  return *this;
}

FraudEmission &FraudEmission::behavior(const fraud::Behavior *value) noexcept {
  behavior_ = value;
  return *this;
}

const fraud::Behavior &FraudEmission::resolvedBehavior() const noexcept {
  return behavior_ != nullptr ? *behavior_ : fraud::kDefaultBehavior;
}

fraud::InjectorRingView FraudEmission::ringView(
    const ::PhantomLedger::entity::person::Topology &topology) const noexcept {
  return fraud::InjectorRingView{
      .profile = profile_,
      .topology = &topology,
  };
}

fraud::InjectorAccountView FraudEmission::accountView(
    const ::PhantomLedger::entity::account::Registry &registry,
    const ::PhantomLedger::entity::account::Ownership &ownership) noexcept {
  return fraud::InjectorAccountView{
      .registry = &registry,
      .ownership = &ownership,
  };
}

fraud::InjectorLegitCounterparties FraudEmission::legitCounterparties(
    const ::PhantomLedger::transfers::legit::ledger::LegitCounterparties
        &counterparties) noexcept {
  return fraud::InjectorLegitCounterparties{
      .billerAccounts = counterparties.billerView(),
      .employers = counterparties.employerView(),
  };
}

} // namespace PhantomLedger::pipeline::stages::transfers
