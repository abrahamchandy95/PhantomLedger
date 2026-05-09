#include "phantomledger/infra/synth/rings.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace PhantomLedger::infra::synth::rings {

namespace {

[[nodiscard]] std::pair<time::TimePoint, time::TimePoint>
sampleWindow(random::Rng &rng, time::Window window, const AccessRules &rules) {
  const std::int32_t totalDays = window.days;

  const std::int32_t burstMin = std::max<std::int32_t>(1, rules.burstDaysMin);
  const std::int32_t burstMax =
      std::max<std::int32_t>(burstMin, rules.burstDaysMax);

  const std::int32_t rawBurst = static_cast<std::int32_t>(
      rng.uniformInt(burstMin, static_cast<std::int64_t>(burstMax) + 1));
  const std::int32_t burstLen = std::min(totalDays, rawBurst);

  const std::int32_t maxStartOffset =
      std::max<std::int32_t>(0, totalDays - burstLen);
  const std::int32_t offset =
      maxStartOffset == 0
          ? 0
          : static_cast<std::int32_t>(rng.uniformInt(
                0, static_cast<std::int64_t>(maxStartOffset) + 1));

  const auto firstSeen = window.start + time::Days{offset};
  const auto lastSeen = firstSeen + time::Days{burstLen - 1};
  return {firstSeen, lastSeen};
}

[[nodiscard]] std::vector<entity::PersonId>
sampleMembers(random::Rng &rng, std::span<const entity::PersonId> members,
              double shareP) {
  if (members.empty()) {
    return {};
  }

  std::vector<entity::PersonId> selected;
  selected.reserve(members.size());
  for (const auto pid : members) {
    if (rng.coin(shareP)) {
      selected.push_back(pid);
    }
  }

  const std::size_t minRequired = std::min<std::size_t>(2, members.size());
  if (selected.size() >= minRequired) {
    return selected;
  }

  selected.clear();
  selected.reserve(minRequired);
  const auto picks = rng.choiceIndices(members.size(), minRequired, false);
  for (const auto idx : picks) {
    selected.push_back(members[idx]);
  }
  return selected;
}

[[nodiscard]] std::span<const entity::PersonId>
membersOf(const entity::person::Ring &ring,
          const entity::person::Topology &topology) {
  if (ring.members.size == 0 ||
      static_cast<std::size_t>(ring.members.offset) + ring.members.size >
          topology.memberStore.size()) {
    return {};
  }
  return std::span<const entity::PersonId>{
      topology.memberStore.data() + ring.members.offset, ring.members.size};
}

} // namespace

std::unordered_map<std::uint32_t, RingPlan>
AccessRules::build(random::Rng &rng, time::Window window,
                   std::span<const entity::person::Ring> rings,
                   const entity::person::Topology &topology) const {
  std::unordered_map<std::uint32_t, RingPlan> plans;
  plans.reserve(rings.size());

  for (const auto &ring : rings) {
    const auto [firstSeen, lastSeen] = sampleWindow(rng, window, *this);
    const auto members = membersOf(ring, topology);

    RingPlan plan{};
    plan.ringId = ring.id;
    plan.firstSeen = firstSeen;
    plan.lastSeen = lastSeen;
    plan.sharedDeviceMembers = sampleMembers(rng, members, sharedDeviceP);
    plan.sharedIpMembers = sampleMembers(rng, members, sharedIpP);

    plans.emplace(ring.id, std::move(plan));
  }

  return plans;
}

} // namespace PhantomLedger::infra::synth::rings
