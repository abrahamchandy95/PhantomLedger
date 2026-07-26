#pragma once

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/entities/parties/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/personas/timeline.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::transfers::fraud {

struct InjectorServices {
  random::Rng &rng;
  const infra::Router *router = nullptr;
  const infra::SharedInfra *ringInfra = nullptr;

  std::uint64_t fraudSeed = 0;
};

struct InjectorRingView {
  const synth::people::Fraud *profile = nullptr;
  const entity::person::Topology *topology = nullptr;

  // H3 part 3c-ii (authority U-8 addendum): the persona-timeline
  // carrier (PersonId-1 indexed). buildPlan derives each ring's alive
  // horizon — the MINIMUM death epoch over its fraud + mule
  // participants — so ring scheduling never recruits the dead. Empty
  // (packs without the carrier) stands the intervals down.
  std::span<const synth::personas::timeline::Timeline> timelines{};
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
