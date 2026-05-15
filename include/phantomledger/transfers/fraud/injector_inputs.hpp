#pragma once

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/primitives/random/rng.hpp"

#include <span>

namespace PhantomLedger::transfers::fraud {

struct InjectorServices {
  random::Rng &rng;
  const infra::Router *router = nullptr;
  const infra::SharedInfra *ringInfra = nullptr;
};

struct InjectorRingView {
  const entities::synth::people::Fraud *profile = nullptr;
  const entity::person::Topology *topology = nullptr;
};

struct InjectorAccountView {
  const entity::account::Registry *registry = nullptr;
  const entity::account::Ownership *ownership = nullptr;
};

struct InjectorLegitCounterparties {
  std::span<const entity::Key> billerAccounts{};
  std::span<const entity::Key> employers{};
};

} // namespace PhantomLedger::transfers::fraud
