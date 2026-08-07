#include "phantomledger/synth/infra/ips.hpp"

#include "phantomledger/entities/infra/enrollment.hpp"
#include "phantomledger/entities/infra/public_endpoints.hpp"
#include "phantomledger/entities/infra/random_ips.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/synth/infra/pool.hpp"
#include "phantomledger/synth/infra/public_pool.hpp"
#include "phantomledger/synth/infra/tenure_table.hpp"
#include "phantomledger/synth/infra/timeline.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PhantomLedger::synth::infra::ips {

namespace {

void registerIfNew(Output &out, std::unordered_set<network::Ipv4> &seen,
                   network::Ipv4 ip, bool blacklisted) {
  if (seen.find(ip) != seen.end()) {
    return;
  }
  seen.insert(ip);
  out.records.push_back(Record{
      .address = ip,
      .blacklisted = blacklisted,
  });
}

void promoteToBlacklisted(Output &out, network::Ipv4 ip) {
  for (auto &rec : out.records) {
    if (rec.address == ip) {
      rec.blacklisted = true;
      return;
    }
  }
}

/* The device-side twin carries the argument: a roster smaller than one
 * endpoint's reach still sits behind carrier NAT, and every gate leg is such a
 * roster. */
constexpr std::size_t kMinCarrierNatLines = 2;

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
      static_cast<std::size_t>(people.count) * 16U / 10U + ringPlans.size();
  out.records.reserve(reserveHint);
  out.usages.reserve(reserveHint);

  std::unordered_set<network::Ipv4> seen;
  seen.reserve(reserveHint);

  for (entity::PersonId p = 1; p <= people.count; ++p) {
    out.byPerson.try_emplace(p);
    out.tenureByPerson.try_emplace(p);
  }

  const auto windowStart = window.start;
  const auto windowDays = window.days;

  // IP LINES AS DHCP LEASES (device-ip-lifecycle, 2026-07-27).
  //
  // Same tiling contract as devices, for the same reason: independent
  // spans left 52.09% of routed IP sessions outside the address's own
  // interval. A residential address is a lease against an ISP pool, not
  // a purchased object, so tenure is months and carries no era axis —
  // see tenure_table.hpp for why that is declared rather than modelled.
  //
  // `nIp` keeps its original meaning: how many addresses are live at
  // once (home broadband, mobile, work). Each is now a chain.
  for (entity::PersonId p = 1; p <= people.count; ++p) {
    const std::uint32_t nIp =
        1U + (rng.coin(extraIpP1) ? 1U : 0U) + (rng.coin(extraIpP2) ? 1U : 0U);

    for (std::uint32_t i = 0; i < nIp; ++i) {
      const auto chain = timeline::sampleChain(
          rng, windowStart, windowDays,
          [](time::TimePoint) { return tenure::ipTenureDays(); });

      for (const auto &link : chain) {
        const auto ip = network::randomIpv4(rng);
        registerIfNew(out, seen, ip, /*blacklisted=*/false);
        out.byPerson[p].push_back(ip);
        out.tenureByPerson[p].push_back(::PhantomLedger::infra::Tenure{
            .firstEpoch = time::toEpochSeconds(link.firstSeen),
            .lastEpochExcl = time::toEpochSeconds(link.lastSeenExcl),
        });
        out.usages.push_back(Usage{
            .personId = p,
            .ipAddress = ip,
            .firstSeen = link.firstSeen,
            .lastSeen = link.lastSeenExcl - time::Days{1},
            .enrolled = ::PhantomLedger::infra::enrollment::ipEnrolled(p, ip),
        });
      }
    }
  }

  /* TWO ISOLATED LANES, KEYED OFF THE RUN SEED. The device generator carries
   * the full argument; it applies verbatim here, and with one extra edge: this
   * pass runs AFTER the device pass off the same `rng`, so a data-dependent
   * count here moves nothing of its own but everything built after it. */
  const random::RngFactory laneFactory{runSeed};

  {
    auto sharedRng = laneFactory.rng({"infra", "ips", "shared"});

    const auto legitPool = buildLegitPool(people);

    std::vector<entity::PersonId> remaining = legitPool;
    std::unordered_map<entity::PersonId, std::size_t> remainingIndex;
    remainingIndex.reserve(remaining.size());
    for (std::size_t i = 0; i < remaining.size(); ++i) {
      remainingIndex[remaining[i]] = i;
    }

    for (const auto anchor : legitPool) {
      if (!pool::contains(remainingIndex, anchor)) {
        continue;
      }
      if (!sharedRng.coin(sharedIpP)) {
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

      std::vector<entity::PersonId> group;
      group.reserve(peers.size() + 1);
      group.push_back(anchor);
      for (const auto pid : peers) {
        group.push_back(pid);
      }

      /* A HOUSEHOLD ADDRESS, NOT AN EPISODE. This used to be a
       * `sampleShortSpan` of at most seven days against a multi-year window,
       * which is why the shared population was correctly wired and
       * statistically invisible. A group that shares a router shares it for as
       * long as they live together, and the address behind it churns on the
       * ordinary residential DHCP clock — the same chain, the same lease
       * length, as any personal line, appended after all of them and
       * contiguous so positional succession still holds. */
      const auto chain = timeline::sampleChain(
          sharedRng, windowStart, windowDays,
          [](time::TimePoint) { return tenure::ipTenureDays(); });

      for (const auto &link : chain) {
        const auto sharedIp = network::randomIpv4(sharedRng);
        registerIfNew(out, seen, sharedIp, /*blacklisted=*/false);

        for (const auto pid : group) {
          out.byPerson[pid].push_back(sharedIp);
          out.tenureByPerson[pid].push_back(::PhantomLedger::infra::Tenure{
              .firstEpoch = time::toEpochSeconds(link.firstSeen),
              .lastEpochExcl = time::toEpochSeconds(link.lastSeenExcl),
          });
          out.usages.push_back(Usage{
              .personId = pid,
              .ipAddress = sharedIp,
              .firstSeen = link.firstSeen,
              .lastSeen = link.lastSeenExcl - time::Days{1},
              .enrolled =
                  ::PhantomLedger::infra::enrollment::ipEnrolled(pid, sharedIp),
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

    const auto sharedIp = network::randomIpv4(rng);

    if (seen.insert(sharedIp).second) {
      out.records.push_back(Record{
          .address = sharedIp,
          .blacklisted = true,
      });
    } else {
      promoteToBlacklisted(out, sharedIp);
    }

    out.ringMap.emplace(plan.ringId, sharedIp);

    for (const auto pid : plan.sharedIpMembers) {
      out.byPerson[pid].push_back(sharedIp);
      out.tenureByPerson[pid].push_back(::PhantomLedger::infra::Tenure{
          .firstEpoch = time::toEpochSeconds(plan.firstSeen),
          .lastEpochExcl = time::toEpochSeconds(plan.lastSeen + time::Days{1}),
      });
      out.usages.push_back(Usage{
          .personId = pid,
          .ipAddress = sharedIp,
          .firstSeen = plan.firstSeen,
          .lastSeen = plan.lastSeen,
          .enrolled =
              ::PhantomLedger::infra::enrollment::ipEnrolled(pid, sharedIp),
      });
    }
  }

  /* CARRIER-GRADE NAT, MINTED LAST AND ON ITS OWN LANE.
   *
   * The IP axis carries the same shortcut as the device axis at roughly 60% of
   * its strength, and it must move in the same round or the model simply
   * learns the other one. This is the address-side twin of the terminal pool:
   * a legitimate endpoint serving tens to hundreds of subscribers, owned by
   * nobody, sitting in exactly the fan-out band that is otherwise pure
   * attacker infrastructure.
   *
   * No `Usage` row and no entry in `byPerson`, for the reasons on the pool
   * type. Resolution is a draw-free point query. */
  {
    auto carrierRng = laneFactory.rng({"infra", "ips", "carrier_nat"});

    const auto lineCount = publics::lineCountFor(
        people.count, peoplePerCarrierNat, kMinCarrierNatLines);

    out.carrierNat = publics::buildPublicPool<network::Ipv4>(
        carrierRng, window,
        publics::PoolSpec{
            .lineCount = lineCount,
            .people = people.count,
            .maxUsersPerLine = maxUsersPerCarrierNat,
            .tenureDays = tenure::carrierNatTenureDays(),
            .rowShare = carrierNatRowShare,
            .poolDomain =
                ::PhantomLedger::infra::publicEndpoints::kCarrierNatPoolDomain,
        },
        [&](std::size_t, std::size_t) {
          const auto ip = network::randomIpv4(carrierRng);
          registerIfNew(out, seen, ip, /*blacklisted=*/false);
          return ip;
        });
  }

  return out;
}

} // namespace PhantomLedger::synth::infra::ips
