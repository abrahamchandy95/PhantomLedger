#include "phantomledger/synth/infra/devices.hpp"

#include "phantomledger/entities/infra/enrollment.hpp"
#include "phantomledger/synth/infra/pool.hpp"
#include "phantomledger/synth/infra/tenure_table.hpp"
#include "phantomledger/synth/infra/timeline.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::infra::devices {

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
    out.tenureByPerson.try_emplace(p);
  }

  const auto windowStart = window.start;
  const auto windowDays = window.days;

  // DEVICE LINES, NOT SINGLE DEVICES (device-ip-lifecycle, 2026-07-27).
  //
  // A person holds `nLines` devices CONCURRENTLY (a phone, maybe also a
  // laptop), and each line is a REPLACEMENT CHAIN over the window rather
  // than one device held forever. `nLines` keeps the old
  // `secondDeviceP` draw and its meaning — how many endpoints are live
  // at once — so the concurrency distribution is unchanged; what changes
  // is that each line now turns over.
  //
  // Chains TILE the window, which is the property the router needs: at
  // every instant exactly one device per line is live, so filtering by
  // timestamp can never come up empty. Before this, spans were
  // independent random sub-intervals and 53.51% of routed sessions fell
  // outside the endpoint's own interval.
  //
  // Lines are emitted CONTIGUOUSLY into byPerson (line 0's whole chain,
  // then line 1's). The router relies on that: when a device's tenure
  // ends, its successor is the NEXT pool index, so "advance to the next
  // live index" is exactly replacement-within-a-line and needs no line
  // metadata on the hot path.
  //
  // Slot numbers run 1..N across the whole person, not per line, so
  // every Identity stays distinct. Slot is an opaque discriminator here,
  // never an ordinal a model should read — renderDeviceId maps all of
  // them into one fixed-width namespace.
  for (entity::PersonId p = 1; p <= people.count; ++p) {
    const std::uint32_t nLines = rng.coin(secondDeviceP) ? 2U : 1U;

    std::uint32_t slot = 1;
    for (std::uint32_t line = 0; line < nLines; ++line) {
      const auto chain = timeline::sampleChain(
          rng, windowStart, windowDays,
          [](time::TimePoint at) { return tenure::deviceTenureDays(at); });

      for (const auto &link : chain) {
        const auto id = DeviceIdentity::person(p, slot);
        ++slot;
        const auto kind = sampleKind(rng);

        registerRecord(out, id, kind, /*flagged=*/false);
        out.byPerson[p].push_back(id);
        out.tenureByPerson[p].push_back(::PhantomLedger::infra::Tenure{
            .firstEpoch = time::toEpochSeconds(link.firstSeen),
            .lastEpochExcl = time::toEpochSeconds(link.lastSeenExcl),
        });

        // Usage keeps its INCLUSIVE last-seen contract for the exporters
        // (HAS_USED, the AML edges); the half-open form lives in Tenure.
        out.usages.push_back(Usage{
            .personId = p,
            .deviceId = id,
            .firstSeen = link.firstSeen,
            .lastSeen = link.lastSeenExcl - time::Days{1},
            .enrolled = ::PhantomLedger::infra::enrollment::deviceEnrolled(p, id),
        });
      }
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

    // Shared endpoints keep their SHORT independent span — a borrowed
    // device genuinely is an episode, not a line the person owns. They
    // are appended AFTER every personal line, so they never break the
    // "next index is my replacement" rule inside a line, and they are
    // reachable only while live.
    for (const auto pid : group) {
      out.byPerson[pid].push_back(sharedId);
      out.tenureByPerson[pid].push_back(::PhantomLedger::infra::Tenure{
          .firstEpoch = time::toEpochSeconds(firstSeen),
          .lastEpochExcl = time::toEpochSeconds(lastSeen + time::Days{1}),
      });
      out.usages.push_back(Usage{
          .personId = pid,
          .deviceId = sharedId,
          .firstSeen = firstSeen,
          .lastSeen = lastSeen,
          .enrolled =
              ::PhantomLedger::infra::enrollment::deviceEnrolled(pid, sharedId),
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
      out.tenureByPerson[pid].push_back(::PhantomLedger::infra::Tenure{
          .firstEpoch = time::toEpochSeconds(plan.firstSeen),
          .lastEpochExcl = time::toEpochSeconds(plan.lastSeen + time::Days{1}),
      });
      out.usages.push_back(Usage{
          .personId = pid,
          .deviceId = sharedId,
          .firstSeen = plan.firstSeen,
          .lastSeen = plan.lastSeen,
          .enrolled =
              ::PhantomLedger::infra::enrollment::deviceEnrolled(pid, sharedId),
      });
    }
  }

  return out;
}

} // namespace PhantomLedger::synth::infra::devices
