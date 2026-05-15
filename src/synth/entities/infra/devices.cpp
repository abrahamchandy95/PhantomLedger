#include "phantomledger/synth/entities/infra/devices.hpp"

#include "phantomledger/synth/entities/infra/pool.hpp"
#include "phantomledger/synth/entities/infra/timeline.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::infra::synth::devices {

using DeviceIdentity = ::PhantomLedger::devices::Identity;

namespace {

[[nodiscard]] DeviceKind sampleKind(random::Rng &rng) {
  const auto idx = rng.choiceIndex(kAllDeviceKinds.size());
  return kAllDeviceKinds[idx];
}

void registerRecord(Output &out, DeviceIdentity id, DeviceKind kind,
                    bool flagged) {
  out.records.push_back(Record{
      .identity = id,
      .kind = kind,
      .flagged = flagged,
  });
}

[[nodiscard]] std::vector<entity::PersonId>
buildLegitPool(const entity::person::Roster &people) {
  std::vector<entity::PersonId> out;
  out.reserve(people.count);
  for (entity::PersonId p = 1; p <= people.count; ++p) {
    if (!people.has(p, entity::person::Flag::fraud)) {
      out.push_back(p);
    }
  }
  return out;
}

} // namespace

Output AssignmentRules::build(
    random::Rng &rng, time::Window window, const entity::person::Roster &people,
    const std::unordered_map<std::uint32_t, RingPlan> &ringPlans) const {
  Output out;
  const auto reserveHint =
      static_cast<std::size_t>(people.count) * 12U / 10U + ringPlans.size();
  out.records.reserve(reserveHint);
  out.usages.reserve(reserveHint);

  for (entity::PersonId p = 1; p <= people.count; ++p) {
    out.byPerson.try_emplace(p);
  }

  const auto windowStart = window.start;
  const auto windowDays = window.days;

  for (entity::PersonId p = 1; p <= people.count; ++p) {
    const std::uint32_t nDev = rng.coin(secondDeviceP) ? 2U : 1U;

    for (std::uint32_t slot = 1; slot <= nDev; ++slot) {
      const auto id = DeviceIdentity::person(p, slot);
      const auto kind = sampleKind(rng);

      registerRecord(out, id, kind, /*flagged=*/false);
      out.byPerson[p].push_back(id);

      const auto [firstSeen, lastSeen] =
          timeline::sampleSpan(rng, windowStart, windowDays);
      out.usages.push_back(Usage{
          .personId = p,
          .deviceId = id,
          .firstSeen = firstSeen,
          .lastSeen = lastSeen,
      });
    }
  }

  const auto legitPool = buildLegitPool(people);

  std::vector<entity::PersonId> remaining = legitPool;
  std::unordered_map<entity::PersonId, std::size_t> remainingIndex;
  remainingIndex.reserve(remaining.size());
  for (std::size_t i = 0; i < remaining.size(); ++i) {
    remainingIndex[remaining[i]] = i;
  }

  std::uint64_t legitSharedCounter = 1;

  for (const auto anchor : legitPool) {
    if (!pool::contains(remainingIndex, anchor)) {
      continue;
    }
    if (!rng.coin(legitDeviceNoiseP)) {
      continue;
    }

    pool::swapDelete(remaining, remainingIndex, anchor);

    if (remaining.empty()) {
      break;
    }

    const auto cap = std::min<std::size_t>(remaining.size(), 3U);
    const auto extraCount = static_cast<std::size_t>(
        rng.uniformInt(1, static_cast<std::int64_t>(cap) + 1));

    const auto pickIdx = rng.choiceIndices(remaining.size(), extraCount, false);
    std::vector<entity::PersonId> peers;
    peers.reserve(extraCount);
    for (const auto i : pickIdx) {
      peers.push_back(remaining[i]);
    }

    const auto sharedId =
        ::PhantomLedger::devices::legitShared(legitSharedCounter, 0);
    ++legitSharedCounter;

    const auto kind = sampleKind(rng);
    registerRecord(out, sharedId, kind, /*flagged=*/false);

    const auto [firstSeen, lastSeen] =
        timeline::sampleShortSpan(rng, windowStart, windowDays);

    // The anchor is part of the group too.
    std::vector<entity::PersonId> group;
    group.reserve(peers.size() + 1);
    group.push_back(anchor);
    for (const auto pid : peers) {
      group.push_back(pid);
    }

    for (const auto pid : group) {
      out.byPerson[pid].push_back(sharedId);
      out.usages.push_back(Usage{
          .personId = pid,
          .deviceId = sharedId,
          .firstSeen = firstSeen,
          .lastSeen = lastSeen,
      });
    }

    for (const auto pid : peers) {
      if (pool::contains(remainingIndex, pid)) {
        pool::swapDelete(remaining, remainingIndex, pid);
      }
    }
  }

  std::vector<std::uint32_t> ringIds;
  ringIds.reserve(ringPlans.size());
  for (const auto &kv : ringPlans) {
    ringIds.push_back(kv.first);
  }
  std::sort(ringIds.begin(), ringIds.end());

  for (const auto ringId : ringIds) {
    const auto &plan = ringPlans.at(ringId);

    const auto sharedId = DeviceIdentity::ring(plan.ringId, 0);
    const auto kind = sampleKind(rng);
    registerRecord(out, sharedId, kind, /*flagged=*/true);
    out.ringMap.emplace(plan.ringId, sharedId);

    for (const auto pid : plan.sharedDeviceMembers) {
      out.byPerson[pid].push_back(sharedId);
      out.usages.push_back(Usage{
          .personId = pid,
          .deviceId = sharedId,
          .firstSeen = plan.firstSeen,
          .lastSeen = plan.lastSeen,
      });
    }
  }

  return out;
}

} // namespace PhantomLedger::infra::synth::devices
