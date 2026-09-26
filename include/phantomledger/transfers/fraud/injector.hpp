#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/fraud/behavior.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/fraud/injector_inputs.hpp"

#include <cstddef>
#include <span>

namespace PhantomLedger::transfers::fraud {

// Whether a registry account may be the destination of a ring's camouflage
// P2P cover transfer. The injector builds its camouflage pool with exactly
// this predicate, so a gate can test the pool without a world.
[[nodiscard]] bool camouflageEligible(entity::Key account) noexcept;

class Injector {
public:
  Injector(InjectorServices services, InjectorRingView rings,
           InjectorAccountView accounts, const Behavior &behavior) noexcept;

  [[nodiscard]] InjectionOutput
  inject(time::Window window,
         std::span<const transactions::Transaction> baseTxns) const;

  [[nodiscard]] InjectionOutput
  inject(time::Window window,
         std::span<const transactions::Transaction> baseTxns,
         InjectorLegitCounterparties counterparties) const;

  [[nodiscard]] InjectionOutput
  inject(time::Window window, std::size_t realizedBaseCount,
         InjectorLegitCounterparties counterparties) const;

private:
  InjectorServices services_;
  InjectorRingView rings_{};
  InjectorAccountView accounts_{};
  const Behavior &behavior_;
  random::RngFactory fraudFactory_;
};

} // namespace PhantomLedger::transfers::fraud
