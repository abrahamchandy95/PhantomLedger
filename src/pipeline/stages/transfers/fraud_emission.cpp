#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"

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

fraud::InjectionOutput FraudEmission::inject(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::infra::Router &router,
    const ::PhantomLedger::infra::SharedInfra &ringInfra,
    ::PhantomLedger::time::Window window,
    const ::PhantomLedger::entity::person::Topology &topology,
    const ::PhantomLedger::entity::account::Registry &accounts,
    const ::PhantomLedger::entity::account::Ownership &ownership,
    std::span<const Transaction> draftTxns,
    const ::PhantomLedger::transfers::legit::ledger::TransfersPayload
        &legitPayload) const {
  fraud::Injector injector{
      fraud::Injector::Services{
          .rng = rng,
          .router = &router,
          .ringInfra = &ringInfra,
      },
      programs_.patterns,
  };

  return injector.inject(
      fraud::Injector::RingView{
          .profile = programs_.profile,
          .topology = &topology,
      },
      fraud::Injector::AccountView{
          .registry = &accounts,
          .ownership = &ownership,
      },
      window, draftTxns,
      fraud::Injector::LegitCounterparties{
          .billerAccounts = std::span<const ::PhantomLedger::entity::Key>(
              legitPayload.billerAccounts.data(),
              legitPayload.billerAccounts.size()),
          .employers = std::span<const ::PhantomLedger::entity::Key>(
              legitPayload.employers.data(), legitPayload.employers.size()),
      });
}

} // namespace PhantomLedger::pipeline::stages::transfers
