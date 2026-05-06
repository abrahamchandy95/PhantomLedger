#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"

#include <span>

namespace PhantomLedger::pipeline::stages::transfers {

namespace fraud = ::PhantomLedger::transfers::fraud;

FraudEmission &FraudEmission::programs(Programs value) noexcept {
  programs_ = value;
  return *this;
}

FraudEmission &FraudEmission::profile(
    const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept {
  programs_.profile = value;
  return *this;
}

FraudEmission &FraudEmission::rules(
    ::PhantomLedger::transfers::fraud::Injector::Patterns value) noexcept {
  programs_.patterns = value;
  return *this;
}

fraud::Injector FraudEmission::makeInjector(
    fraud::Injector::Services services, fraud::Injector::RingView rings,
    fraud::Injector::AccountView accounts) const noexcept {
  return fraud::Injector{services, rings, accounts, programs_.patterns};
}

fraud::Injector::RingView FraudEmission::ringView(
    const ::PhantomLedger::entity::person::Topology &topology) const noexcept {
  return fraud::Injector::RingView{
      .profile = programs_.profile,
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
    const ::PhantomLedger::transfers::legit::ledger::TransfersPayload
        &payload) noexcept {
  return fraud::Injector::LegitCounterparties{
      .billerAccounts = std::span<const ::PhantomLedger::entity::Key>(
          payload.billerAccounts.data(), payload.billerAccounts.size()),
      .employers = std::span<const ::PhantomLedger::entity::Key>(
          payload.employers.data(), payload.employers.size()),
  };
}

} // namespace PhantomLedger::pipeline::stages::transfers
