#pragma once

#include "phantomledger/entities/people.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/synth/personas/kinds.hpp"
#include "phantomledger/synth/personas/pack.hpp"
#include "phantomledger/synth/personas/profile.hpp"
#include "phantomledger/synth/personas/seeds.hpp"

#include <cstdint>

namespace PhantomLedger::synth::personas {

[[nodiscard]] inline Pack makePack(random::Rng &rng, std::uint32_t people,
                                   std::uint64_t baseSeed,
                                   const Mix &mix = {}) {
  Pack out;
  out.assignment.byPerson = assign(rng, people, mix);
  out.table.byPerson.reserve(static_cast<std::size_t>(people));

  for (entity::PersonId person = 1; person <= people; ++person) {
    auto local = random::Rng::fromSeed(seed(baseSeed, person));
    out.table.byPerson.push_back(
        profile(local, out.assignment.byPerson[person - 1]));
  }

  return out;
}

[[nodiscard]] inline Pack makePlan(random::Rng &rng,
                                   const entity::person::Roster &people,
                                   std::uint64_t baseSeed,
                                   const Mix &mix = {}) {
  return makePack(rng, people.count, baseSeed, mix);
}

} // namespace PhantomLedger::synth::personas
