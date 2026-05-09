#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"

#include <span>

namespace PhantomLedger::transfers::fraud {

/// Per-call services the injector needs: rng plus shared infra
/// pointers. Pure references / pointers; no ownership.
struct InjectorServices {
  random::Rng &rng;
  const infra::Router *router = nullptr;
  const infra::SharedInfra *ringInfra = nullptr;
};

/// Read-only view of the fraud-ring topology and the linked profile.
struct InjectorRingView {
  const entities::synth::people::Fraud *profile = nullptr;
  const entity::person::Topology *topology = nullptr;
};

/// Read-only view of the account universe used by fraud rings.
struct InjectorAccountView {
  const entity::account::Registry *registry = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

/// Counterparties that legitimate transactions know about and that
/// fraud injection may reference (e.g., billers that camouflage
/// transactions can target).
struct InjectorLegitCounterparties {
  std::span<const entity::Key> billerAccounts{};
  std::span<const entity::Key> employers{};
};

} // namespace PhantomLedger::transfers::fraud
