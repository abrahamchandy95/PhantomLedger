#include "phantomledger/synth/infra/devices.hpp"

#include "phantomledger/entities/infra/enrollment.hpp"
#include "phantomledger/entities/infra/public_endpoints.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/synth/infra/pool.hpp"
#include "phantomledger/synth/infra/public_pool.hpp"
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

/* A world with fewer people than one terminal serves still has public
 * terminals in it, and every gate leg is such a world. Rounding the pool to
 * zero there would leave the population inert exactly where it is measured. */
constexpr std::size_t kMinTerminalLines = 2;

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
    const std::unordered_map<std::uint32_t, RingPlan> &ringPlans,
    std::uint64_t runSeed) const {
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
            .enrolled =
                ::PhantomLedger::infra::enrollment::deviceEnrolled(p, id),
        });
      }
    }
  }

  /* TWO ISOLATED LANES, AND THIS IS THE ROUND'S WHOLE DRAW-DISCIPLINE
   * ARGUMENT.
   *
   * Everything below this point has a draw count that depends on DATA: the
   * household pass spends a coin per still-unattached person plus a
   * data-dependent `choiceIndices` per group, and the terminal pass spends a
   * chain per line with the line count sized off the roster. On the shared
   * entity stream that is the `merchant-churn` rule 2 hazard in its exact
   * original form — a bigger or smaller count here would shift the ring pass,
   * then the whole IP synthesis (which runs next off the same `rng`), and then
   * every draw the transfer stage takes after it.
   *
   * Budgeting a fixed count PER DEVICE would not be enough, because the number
   * of GROUPS is itself data. So both passes run on lanes keyed off the RUN
   * SEED and the main stream spends nothing at all on them. Keying on the run
   * seed rather than on a word drawn from `rng` is what makes these two
   * populations immune to an unrelated upstream change in how many uniforms
   * the entity stream has already spent — which is exactly the failure this
   * project has paid for twice. */
  const random::RngFactory laneFactory{runSeed};

  {
    auto sharedRng = laneFactory.rng({"infra", "devices", "shared"});

    const auto legitPool = buildLegitPool(people);

    std::vector<entity::PersonId> remaining = legitPool;
    std::unordered_map<entity::PersonId, std::size_t> remainingIndex;
    remainingIndex.reserve(remaining.size());
    for (std::size_t i = 0; i < remaining.size(); ++i) {
      remainingIndex[remaining[i]] = i;
    }

    std::uint64_t sharedGroupCounter = 1;

    for (const auto anchor : legitPool) {
      if (!pool::contains(remainingIndex, anchor)) {
        continue;
      }
      if (!sharedRng.coin(sharedDeviceP)) {
        continue;
      }

      pool::swapDelete(remaining, remainingIndex, anchor);

      if (remaining.empty()) {
        break;
      }

      const auto cap = std::min<std::size_t>(
          remaining.size(), static_cast<std::size_t>(sharedGroupMaxExtra));
      const auto extraCount = static_cast<std::size_t>(
          sharedRng.uniformInt(1, static_cast<std::int64_t>(cap) + 1));

      const auto pickIdx =
          sharedRng.choiceIndices(remaining.size(), extraCount, false);
      std::vector<entity::PersonId> peers;
      peers.reserve(extraCount);
      for (const auto i : pickIdx) {
        peers.push_back(remaining[i]);
      }

      // The anchor is part of the group too.
      std::vector<entity::PersonId> group;
      group.reserve(peers.size() + 1);
      group.push_back(anchor);
      for (const auto pid : peers) {
        group.push_back(pid);
      }

      /* A HOUSEHOLD LINE, NOT AN EPISODE, and that is the whole of the level
       * change. This device used to get `sampleShortSpan` — a tenure bounded
       * at min(days, 7) DAYS, 0.48% of a four-year window — so the population
       * existed, was correctly wired into the router, and carried 0.0065% of
       * card-view rows. A family tablet is a line the household owns.
       *
       * It is a CHAIN on the same era-dated replacement clock as a personal
       * line, for two reasons. It tiles, so the group holds a shared endpoint
       * continuously instead of once; and a household replacing its tablet is
       * the same event as a person replacing their phone, so modelling it with
       * a different law would be a claim this file cannot support.
       *
       * Links are appended AFTER every personal line and CONTIGUOUSLY in chain
       * order, which is what the router's positional succession requires: when
       * a shared link retires, the next index IS its replacement. */
      const auto chain = timeline::sampleChain(
          sharedRng, windowStart, windowDays,
          [](time::TimePoint at) { return tenure::deviceTenureDays(at); });

      const auto groupId = sharedGroupCounter;
      ++sharedGroupCounter;

      std::uint32_t slot = 0;
      for (const auto &link : chain) {
        const auto sharedId =
            ::PhantomLedger::devices::legitShared(groupId, slot);
        ++slot;

        const auto kind = sampleKind(sharedRng);
        registerRecord(out, sharedId, kind, /*flagged=*/false);

        for (const auto pid : group) {
          out.byPerson[pid].push_back(sharedId);
          out.tenureByPerson[pid].push_back(::PhantomLedger::infra::Tenure{
              .firstEpoch = time::toEpochSeconds(link.firstSeen),
              .lastEpochExcl = time::toEpochSeconds(link.lastSeenExcl),
          });
          out.usages.push_back(Usage{
              .personId = pid,
              .deviceId = sharedId,
              .firstSeen = link.firstSeen,
              .lastSeen = link.lastSeenExcl - time::Days{1},
              .enrolled = ::PhantomLedger::infra::enrollment::deviceEnrolled(
                  pid, sharedId),
          });
        }
      }

      for (const auto pid : peers) {
        if (pool::contains(remainingIndex, pid)) {
          pool::swapDelete(remaining, remainingIndex, pid);
        }
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

  /* PUBLIC TERMINALS, MINTED LAST AND ON THEIR OWN LANE.
   *
   * This is the population that fills the empty stretch between a household
   * device and the attacker cluster, and it does it WITHOUT touching the
   * attacker rail: a terminal is entirely legitimate, carries hundreds of
   * distinct cards, and is owned by nobody.
   *
   * NO `Usage` ROW AND NO ENTRY IN `byPerson`, DELIBERATELY. Nobody owns a
   * kiosk, so there is no ownership edge to write, and an entry in a person's
   * own pool would hand it a third to a half of that person's traffic through
   * the router's sticky random walk. Resolution is a draw-free point query on
   * the pool; see `entities/infra/public_endpoints.hpp`.
   *
   * The kind is fixed rather than sampled. A public terminal is a fixed
   * workstation, not a handset, and fixing it also keeps the mint free of
   * draws so the pool's uniform budget is exactly the reach weight plus the
   * chain. */
  {
    auto terminalRng = laneFactory.rng({"infra", "devices", "terminals"});

    const auto lineCount = publics::lineCountFor(
        people.count, peoplePerTerminal, kMinTerminalLines);

    out.terminals = publics::buildPublicPool<DeviceIdentity>(
        terminalRng, window,
        publics::PoolSpec{
            .lineCount = lineCount,
            .people = people.count,
            .maxUsersPerLine = maxUsersPerTerminal,
            .tenureDays = tenure::publicTerminalTenureDays(),
            .rowShare = terminalRowShare,
            .poolDomain =
                ::PhantomLedger::infra::publicEndpoints::kTerminalPoolDomain,
        },
        [&out](std::size_t line, std::size_t link) {
          const auto id = DeviceIdentity::publicTerminal(
              ::PhantomLedger::infra::kPublicTerminalOwnerIdBase +
                  static_cast<std::uint64_t>(line),
              static_cast<std::uint32_t>(link));
          registerRecord(out, id, DeviceKind::desktop, /*flagged=*/false);
          return id;
        });
  }

  return out;
}

} // namespace PhantomLedger::synth::infra::devices
