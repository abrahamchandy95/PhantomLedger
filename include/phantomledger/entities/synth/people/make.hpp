#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/entities/people/people.hpp"
#include "phantomledger/entities/synth/people/flags.hpp"
#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/people/rings.hpp"
#include "phantomledger/entities/synth/people/topology.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace PhantomLedger::entities::synth::people {

[[nodiscard]] inline Pack make(random::Rng &rng, int population,
                               const Fraud &fraud = {}) {
  if (population < 0) {
    throw std::invalid_argument("population must be >= 0");
  }

  Pack out;
  out.roster.count = static_cast<std::uint32_t>(population);
  out.roster.flags.assign(static_cast<std::size_t>(population), 0);

  if (population == 0) {
    return out;
  }

  const int ringCount = sampleRingCount(rng, fraud, population);
  const int ceiling = fraud.maxParticipants(population);

  std::vector<int> sizes;
  std::vector<int> muleCounts;
  std::vector<int> victimCounts;

  int consumed = 0;
  for (int ring = 0; ring < ringCount; ++ring) {
    RingPlan plan{};
    if (!sampleRing(rng, fraud, ceiling - consumed, population - consumed,
                    plan)) {
      break;
    }

    sizes.push_back(plan.size);
    muleCounts.push_back(plan.mules);
    victimCounts.push_back(plan.victims);
    consumed += plan.size;
  }

  int ringPeople = 0;
  for (int size : sizes) {
    ringPeople += size;
  }

  std::vector<entity::PersonId> ringPool;
  std::vector<entity::PersonId> nonRing;

  if (ringPeople > 0) {
    if (ringPeople >= population) {
      throw std::invalid_argument("fraud ring participants exceed population");
    }

    std::vector<std::uint8_t> inRing(static_cast<std::size_t>(population) + 1,
                                     0);
    const auto pool =
        rng.choiceIndices(static_cast<std::size_t>(population),
                          static_cast<std::size_t>(ringPeople), false);

    ringPool.reserve(pool.size());
    for (const auto idx : pool) {
      const auto person = static_cast<entity::PersonId>(idx + 1);
      ringPool.push_back(person);
      inRing[person] = 1;
    }

    nonRing.reserve(static_cast<std::size_t>(population - ringPeople));
    for (entity::PersonId person = 1; person <= out.roster.count; ++person) {
      if (!inRing[person]) {
        nonRing.push_back(person);
      }
    }
  } else {
    nonRing.reserve(static_cast<std::size_t>(population));
    for (entity::PersonId person = 1; person <= out.roster.count; ++person) {
      nonRing.push_back(person);
    }
  }

  std::vector<TempRing> rings;
  rings.reserve(sizes.size());

  std::vector<std::vector<std::uint32_t>> muleHomes(
      static_cast<std::size_t>(population) + 1);
  std::vector<entity::PersonId> priorVictims;
  int cursor = 0;

  for (std::size_t rid = 0; rid < sizes.size(); ++rid) {
    TempRing ring;
    ring.members.assign(ringPool.begin() + cursor,
                        ringPool.begin() + cursor + sizes[rid]);
    cursor += sizes[rid];

    std::vector<std::uint8_t> isMule(static_cast<std::size_t>(sizes[rid]), 0);
    const auto muleIdx =
        rng.choiceIndices(static_cast<std::size_t>(sizes[rid]),
                          static_cast<std::size_t>(muleCounts[rid]), false);
    for (const auto idx : muleIdx) {
      isMule[idx] = 1;
      ring.mules.push_back(ring.members[idx]);
    }

    for (int i = 0; i < sizes[rid]; ++i) {
      if (!isMule[static_cast<std::size_t>(i)]) {
        ring.frauds.push_back(ring.members[static_cast<std::size_t>(i)]);
      }
    }

    if (ring.frauds.empty() && !ring.mules.empty()) {
      const auto promoted = ring.mules.back();
      ring.mules.pop_back();
      ring.frauds.push_back(promoted);
    }

    for (const auto mule : ring.mules) {
      muleHomes[mule].push_back(static_cast<std::uint32_t>(rid));
    }

    const int availableVictims =
        std::min(victimCounts[rid], static_cast<int>(nonRing.size()));
    if (availableVictims > 0) {
      const auto victimIdx = rng.choiceIndices(
          nonRing.size(), static_cast<std::size_t>(availableVictims), false);
      ring.victims.reserve(victimIdx.size());
      for (const auto idx : victimIdx) {
        ring.victims.push_back(nonRing[idx]);
      }
    }

    if (!priorVictims.empty()) {
      for (auto &victim : ring.victims) {
        if (rng.coin(fraud.victims.repeatP)) {
          victim = priorVictims[rng.choiceIndex(priorVictims.size())];
        }
      }
    }

    priorVictims.insert(priorVictims.end(), ring.victims.begin(),
                        ring.victims.end());

    for (const auto person : ring.frauds) {
      set(out.roster.flags, person, entity::people::Flag::fraud);
    }
    for (const auto person : ring.mules) {
      set(out.roster.flags, person, entity::people::Flag::mule);
    }
    for (const auto person : ring.victims) {
      set(out.roster.flags, person, entity::people::Flag::victim);
    }

    rings.push_back(std::move(ring));
  }

  injectMultiRingMules(rng, fraud, rings, muleHomes, out.roster.count);

  int soloCount =
      sampleSolos(rng, fraud, population, std::max(0, ceiling - ringPeople));
  if (soloCount > 0) {
    std::vector<std::uint8_t> inRing(static_cast<std::size_t>(population) + 1,
                                     0);
    for (const auto &ring : rings) {
      for (const auto person : ring.members) {
        inRing[person] = 1;
      }
    }

    std::vector<entity::PersonId> eligible;
    eligible.reserve(static_cast<std::size_t>(population));
    for (entity::PersonId person = 1; person <= out.roster.count; ++person) {
      if (!inRing[person]) {
        eligible.push_back(person);
      }
    }

    soloCount = std::min(soloCount, static_cast<int>(eligible.size()));
    if (soloCount > 0) {
      const auto soloIdx = rng.choiceIndices(
          eligible.size(), static_cast<std::size_t>(soloCount), false);
      for (const auto idx : soloIdx) {
        const auto person = eligible[idx];
        set(out.roster.flags, person, entity::people::Flag::fraud);
        set(out.roster.flags, person, entity::people::Flag::soloFraud);
      }
    }
  }

  flatten(rings, out.topology);
  return out;
}

} // namespace PhantomLedger::entities::synth::people
