#pragma once

#include "phantomledger/entities/people/ring.hpp"
#include "phantomledger/entities/people/topology.hpp"
#include "phantomledger/entities/synth/people/rings.hpp"

namespace PhantomLedger::entities::synth::people {

inline void flatten(const std::vector<TempRing> &rings,
                    entities::people::Topology &out) {
  out.rings.reserve(rings.size());

  std::size_t memberTotal = 0;
  std::size_t fraudTotal = 0;
  std::size_t muleTotal = 0;
  std::size_t victimTotal = 0;

  for (const auto &ring : rings) {
    memberTotal += ring.members.size();
    fraudTotal += ring.frauds.size();
    muleTotal += ring.mules.size();
    victimTotal += ring.victims.size();
  }

  out.memberStore.reserve(memberTotal);
  out.fraudStore.reserve(fraudTotal);
  out.muleStore.reserve(muleTotal);
  out.victimStore.reserve(victimTotal);

  for (std::uint32_t ringId = 0;
       ringId < static_cast<std::uint32_t>(rings.size()); ++ringId) {
    const auto &src = rings[ringId];

    const entities::people::Slice members{
        .offset = static_cast<std::uint32_t>(out.memberStore.size()),
        .size = static_cast<std::uint32_t>(src.members.size()),
    };
    out.memberStore.insert(out.memberStore.end(), src.members.begin(),
                           src.members.end());

    const entities::people::Slice frauds{
        .offset = static_cast<std::uint32_t>(out.fraudStore.size()),
        .size = static_cast<std::uint32_t>(src.frauds.size()),
    };
    out.fraudStore.insert(out.fraudStore.end(), src.frauds.begin(),
                          src.frauds.end());

    const entities::people::Slice mules{
        .offset = static_cast<std::uint32_t>(out.muleStore.size()),
        .size = static_cast<std::uint32_t>(src.mules.size()),
    };
    out.muleStore.insert(out.muleStore.end(), src.mules.begin(),
                         src.mules.end());

    const entities::people::Slice victims{
        .offset = static_cast<std::uint32_t>(out.victimStore.size()),
        .size = static_cast<std::uint32_t>(src.victims.size()),
    };
    out.victimStore.insert(out.victimStore.end(), src.victims.begin(),
                           src.victims.end());

    out.rings.push_back(entities::people::Ring{
        .id = ringId,
        .members = members,
        .frauds = frauds,
        .mules = mules,
        .victims = victims,
    });
  }
}

} // namespace PhantomLedger::entities::synth::people
