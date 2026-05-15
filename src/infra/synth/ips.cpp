#include "phantomledger/infra/synth/ips.hpp"

#include "phantomledger/infra/synth/pool.hpp"
#include "phantomledger/infra/synth/timeline.hpp"
#include "phantomledger/synth/entities/infra/random_ips.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PhantomLedger::infra::synth::ips {

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
      static_cast<std::size_t>(people.count) * 16U / 10U + ringPlans.size();
  out.records.reserve(reserveHint);
  out.usages.reserve(reserveHint);

  std::unordered_set<network::Ipv4> seen;
  seen.reserve(reserveHint);

  for (entity::PersonId p = 1; p <= people.count; ++p) {
    out.byPerson.try_emplace(p);
  }

  const auto windowStart = window.start;
  const auto windowDays = window.days;

  for (entity::PersonId p = 1; p <= people.count; ++p) {
    const std::uint32_t nIp =
        1U + (rng.coin(extraIpP1) ? 1U : 0U) + (rng.coin(extraIpP2) ? 1U : 0U);

    for (std::uint32_t i = 0; i < nIp; ++i) {
      const auto ip = network::randomIpv4(rng);
      registerIfNew(out, seen, ip, /*blacklisted=*/false);
      out.byPerson[p].push_back(ip);

      const auto [firstSeen, lastSeen] =
          timeline::sampleSpan(rng, windowStart, windowDays);
      out.usages.push_back(Usage{
          .personId = p,
          .ipAddress = ip,
          .firstSeen = firstSeen,
          .lastSeen = lastSeen,
      });
    }
  }

  // legit-shared "noise" IPs
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
    if (!rng.coin(legitIpNoiseP)) {
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

    const auto sharedIp = network::randomIpv4(rng);
    registerIfNew(out, seen, sharedIp, /*blacklisted=*/false);

    const auto [firstSeen, lastSeen] =
        timeline::sampleShortSpan(rng, windowStart, windowDays);

    std::vector<entity::PersonId> group;
    group.reserve(peers.size() + 1);
    group.push_back(anchor);
    for (const auto pid : peers) {
      group.push_back(pid);
    }

    for (const auto pid : group) {
      out.byPerson[pid].push_back(sharedIp);
      out.usages.push_back(Usage{
          .personId = pid,
          .ipAddress = sharedIp,
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
      out.usages.push_back(Usage{
          .personId = pid,
          .ipAddress = sharedIp,
          .firstSeen = plan.firstSeen,
          .lastSeen = plan.lastSeen,
      });
    }
  }

  return out;
}

} // namespace PhantomLedger::infra::synth::ips
