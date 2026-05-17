#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/accounts/assign.hpp"
#include "phantomledger/synth/accounts/pack.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::synth::accounts {

using identifiers::Bank;
using identifiers::Role;

struct BusinessOwnerPlan {

  double businessOwnerProbability = 0.08;

  double secondBusinessProbability = 0.15;

  std::uint64_t businessSerialBase = 1'000'000ULL;
};

inline void assignBusinessOwners(Pack &accounts,
                                 const entity::person::Roster &roster,
                                 random::Rng &rng,
                                 const BusinessOwnerPlan &plan = {}) {
  if (plan.businessOwnerProbability <= 0.0 || roster.count == 0) {
    return;
  }

  struct OwnedBusiness {
    entity::Key key;
    entity::PersonId owner;
  };

  std::vector<OwnedBusiness> owned;
  owned.reserve(static_cast<std::size_t>(roster.count *
                                         plan.businessOwnerProbability * 1.2));

  std::uint64_t serial = plan.businessSerialBase;

  for (entity::PersonId person = 1; person <= roster.count; ++person) {
    if (!rng.coin(plan.businessOwnerProbability)) {
      continue;
    }

    owned.push_back(
        {entity::makeKey(Role::business, Bank::internal, serial++), person});

    if (rng.coin(plan.secondBusinessProbability)) {
      owned.push_back(
          {entity::makeKey(Role::business, Bank::internal, serial++), person});
    }
  }

  assignOwners(
      accounts, owned, [](const OwnedBusiness &r) { return r.key; },
      [](const OwnedBusiness &r) { return r.owner; }, roster.count,
      /*external=*/false);
}

} // namespace PhantomLedger::synth::accounts
