#pragma once

#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/personas/timeline.hpp"
#include "phantomledger/transfers/fraud/injector_inputs.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <span>

namespace PhantomLedger::transfers::fraud {

struct Behavior;
} // namespace PhantomLedger::transfers::fraud

namespace PhantomLedger::pipeline::stages::transfers {

class FraudEmission {
public:
  FraudEmission() = default;

  FraudEmission &
  profile(const ::PhantomLedger::synth::people::Fraud *value) noexcept;

  FraudEmission &
  behavior(const ::PhantomLedger::transfers::fraud::Behavior *value) noexcept;

  [[nodiscard]] const ::PhantomLedger::transfers::fraud::Behavior &
  resolvedBehavior() const noexcept;

  // H3 part 3c-ii: `timelines` (the personas pack's carrier) gives the
  // injector each ring's alive horizon — ring scheduling never
  // recruits the dead. Empty (the default) stands the intervals down.
  [[nodiscard]] ::PhantomLedger::transfers::fraud::InjectorRingView ringView(
      const ::PhantomLedger::entity::person::Topology &topology,
      std::span<const ::PhantomLedger::synth::personas::timeline::Timeline>
          timelines = {}) const noexcept;

  [[nodiscard]] static ::PhantomLedger::transfers::fraud::InjectorAccountView
  accountView(
      const ::PhantomLedger::entity::account::Registry &registry,
      const ::PhantomLedger::entity::account::Ownership &ownership) noexcept;

  // card-fraud-realism-v2 step b: `merchants` (the entity stage's
  // acceptance catalogue) and `homeAreas` (People's compact per-person
  // home carrier) are the inputs the card rails need in order to select
  // their destination from the SAME population — on the SAME geographic
  // axis — that legitimate card purchases use. See
  // transfers/fraud/injector_inputs.hpp for the defect they close.
  //
  // ALL DEFAULT TO ABSENT, and absent means "behave exactly as before".
  // FOUR call sites exist — simulate.cpp (the monolith reference),
  // windowed_run.cpp (production), window_leg_support.hpp (the gate
  // harness) and test_membership.cpp — and the two ENGINES must pass
  // IDENTICAL arguments or test_arch_equivalence / test_production_
  // windowed will diverge. Every carrier round updates all of the sites
  // that read it together, for exactly that reason.
  [[nodiscard]] static ::PhantomLedger::transfers::fraud::
      InjectorLegitCounterparties
      legitCounterparties(
          const ::PhantomLedger::transfers::legit::ledger::LegitCounterparties
              &counterparties,
          const ::PhantomLedger::entity::merchant::Catalog *merchants = nullptr,
          std::span<const ::PhantomLedger::entity::geography::GeoAreaId>
              homeAreas = {},
          // THE VICTIM-SIDE CARRIER, one pointer for the whole victim
          // model (victimization-v2 + v3):
          //   * v2 DERIVES the per-person card-exposure weights from
          //     `personas->table` HERE rather than at each call site —
          //     one derivation, so the two engines cannot compute it
          //     differently on the path test_arch_equivalence guards.
          //   * v3 forwards the pack itself, because the scam-rail
          //     hazard needs persona-at-date, age-at-date and the join
          //     day TOGETHER; three spans would be three chances for a
          //     site to fill some and not others.
          // nullptr leaves victim selection on the pre-v2 uniform draw,
          // bit-identically, with no severity grading and no membership
          // gate.
          const ::PhantomLedger::synth::personas::Pack *personas = nullptr);

private:
  const ::PhantomLedger::synth::people::Fraud *profile_ = nullptr;
  const ::PhantomLedger::transfers::fraud::Behavior *behavior_ = nullptr;
};

} // namespace PhantomLedger::pipeline::stages::transfers
