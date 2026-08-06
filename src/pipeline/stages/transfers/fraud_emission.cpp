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
    const ::PhantomLedger::entity::person::Topology &topology,
    std::span<const ::PhantomLedger::synth::personas::timeline::Timeline>
        timelines) const noexcept {
  return fraud::InjectorRingView{
      .profile = profile_,
      .topology = &topology,
      .timelines = timelines,
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
        &counterparties,
    const ::PhantomLedger::entity::merchant::Catalog *merchants,
    std::span<const ::PhantomLedger::entity::geography::GeoAreaId> homeAreas,
    const ::PhantomLedger::synth::personas::Pack *personas,
    const ::PhantomLedger::entity::parties::relocation::Schedule *relocation) {
  /* The merchant acceptance catalogue and the home-area axis ride alongside
   * the legit pools; all of these are borrowed and all default to absent.
   *
   * THE ONE DERIVATION: everything victim-side that has to be COMPUTED is
   * computed here, once, from a single pointer — never at the call sites. That
   * is what makes the two engines structurally incapable of disagreeing about
   * it. */
  return fraud::InjectorLegitCounterparties{
      .billerAccounts = counterparties.billerView(),
      .employers = counterparties.employerView(),
      .merchants = merchants,
      .homeAreas = homeAreas,
      /* The history the unauthorized planner resolves at each case date. */
      .relocation = relocation,
      /* The pack itself, for the scam-rail hazard (persona-at-date x
       * age-at-date), the age-graded severity and the membership gate. */
      .personas = personas,
      /* Derived HERE for every call site. An absent pack yields an empty
       * vector, and the picker branches on empty to keep the plain uniform
       * draw bit-identical. */
      .cardExposure = personas != nullptr
                          ? fraud::cardExposureWeights(personas->table)
                          : std::vector<double>{},
  };
}

} // namespace PhantomLedger::pipeline::stages::transfers
