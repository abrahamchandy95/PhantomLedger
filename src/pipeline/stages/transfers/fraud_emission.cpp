#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"

namespace PhantomLedger::pipeline::stages::transfers {

namespace fraud = ::PhantomLedger::transfers::fraud;

FraudEmission &FraudEmission::profile(
    const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept {
  profile_ = value;
  return *this;
}

FraudEmission &
FraudEmission::typologyWeights(fraud::TypologyWeights value) noexcept {
  typologyWeights_ = value;
  return *this;
}

FraudEmission &FraudEmission::layeringRules(
    fraud::typologies::layering::Rules value) noexcept {
  layeringRules_ = value;
  return *this;
}

FraudEmission &FraudEmission::structuringRules(
    fraud::typologies::structuring::Rules value) noexcept {
  structuringRules_ = value;
  return *this;
}

FraudEmission &
FraudEmission::camouflageRates(fraud::camouflage::Rates value) noexcept {
  camouflageRates_ = value;
  return *this;
}

fraud::Injector FraudEmission::makeInjector(
    fraud::Injector::Services services, fraud::Injector::RingView rings,
    fraud::Injector::AccountView accounts) const noexcept {
  fraud::Injector injector{services, rings, accounts};
  injector.typologyWeights(typologyWeights_);
  injector.layeringRules(layeringRules_);
  injector.structuringRules(structuringRules_);
  injector.camouflageRates(camouflageRates_);
  return injector;
}

fraud::Injector::RingView FraudEmission::ringView(
    const ::PhantomLedger::entity::person::Topology &topology) const noexcept {
  return fraud::Injector::RingView{
      .profile = profile_,
      .topology = &topology,
  };
}

fraud::Injector::AccountView FraudEmission::accountView(
    const ::PhantomLedger::entity::account::Registry &registry,
    const ::PhantomLedger::entity::account::Ownership &ownership) noexcept {
  return fraud::Injector::AccountView{
      .registry = &registry,
      .ownership = &ownership,
  };
}

fraud::Injector::LegitCounterparties FraudEmission::legitCounterparties(
    const ::PhantomLedger::transfers::legit::ledger::LegitCounterparties
        &counterparties) noexcept {
  return fraud::Injector::LegitCounterparties{
      .billerAccounts = counterparties.billerView(),
      .employers = counterparties.employerView(),
  };
}

} // namespace PhantomLedger::pipeline::stages::transfers
