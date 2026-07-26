#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/synth/people/fraud.hpp"
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

  [[nodiscard]] static ::PhantomLedger::transfers::fraud::
      InjectorLegitCounterparties
      legitCounterparties(
          const ::PhantomLedger::transfers::legit::ledger::LegitCounterparties
              &counterparties) noexcept;

private:
  const ::PhantomLedger::synth::people::Fraud *profile_ = nullptr;
  const ::PhantomLedger::transfers::fraud::Behavior *behavior_ = nullptr;
};

} // namespace PhantomLedger::pipeline::stages::transfers
