#include "phantomledger/synth/pii/correlate.hpp"

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace PhantomLedger::synth::pii {
namespace {

void appendSlice(std::span<const entity::PersonId> store,
                 const entity::person::Slice &slice,
                 std::vector<entity::PersonId> &out) {
  const std::size_t begin = slice.offset;
  const std::size_t end = std::min(begin + slice.size, store.size());

  if (begin < end) {
    auto sub = store.subspan(begin, end - begin);
    out.insert(out.end(), sub.begin(), sub.end());
  }
}

[[nodiscard]] std::vector<entity::PersonId>
correlatableMembers(const entity::person::Topology &topology,
                    const entity::person::Ring &ring, bool includeVictims) {
  std::vector<entity::PersonId> out;
  out.reserve(ring.frauds.size + ring.mules.size +
              (includeVictims ? ring.victims.size : 0U));

  appendSlice(topology.fraudStore, ring.frauds, out);
  appendSlice(topology.muleStore, ring.mules, out);
  if (includeVictims) {
    appendSlice(topology.victimStore, ring.victims, out);
  }
  return out;
}

[[nodiscard]] entity::pii::Record &recordOf(entity::pii::Roster &roster,
                                            entity::PersonId p) noexcept {
  return roster.records[static_cast<std::size_t>(p) - 1U];
}

template <class CopyFunc>
void shareAttribute(random::Rng &rng, entity::pii::Roster &roster,
                    const std::vector<entity::PersonId> &members,
                    double shareProb, double coverage,
                    std::size_t maxMembersPerValue, std::size_t minMembers,
                    CopyFunc copyFunc) {
  if (members.size() < minMembers || !rng.coin(shareProb)) {
    return;
  }

  // Calculate required count and clamp it between 2 and the absolute maximums
  std::size_t count = static_cast<std::size_t>(coverage * members.size() + 0.5);
  count = std::clamp(count, std::size_t{2},
                     std::min(members.size(), maxMembersPerValue));

  const auto chosen = rng.choiceIndices(members.size(), count, false);
  if (chosen.empty()) {
    return;
  }

  const auto &anchor = recordOf(roster, members[chosen.front()]);
  for (std::size_t k = 1; k < chosen.size(); ++k) {
    copyFunc(anchor, recordOf(roster, members[chosen[k]]));
  }
}

void correlateRing(random::Rng &rng, const Sharing &cfg,
                   entity::pii::Roster &roster,
                   const std::vector<entity::PersonId> &members) {
  const auto cap = cfg.limits.maxMembersPerValue;
  const auto lo = cfg.limits.minMembers;

  // Helper lambda to clean up the repetitive shareAttribute calls
  auto share = [&](const auto &prob, const auto &cov, auto copyLogic) {
    shareAttribute(rng, roster, members, prob, cov, cap, lo, copyLogic);
  };

  share(cfg.probability.phone, cfg.coverage.phone,
        [](const auto &from, auto &to) { to.phone = from.phone; });

  share(cfg.probability.email, cfg.coverage.email,
        [](const auto &from, auto &to) { to.email = from.email; });

  share(cfg.probability.address, cfg.coverage.address,
        [](const auto &from, auto &to) { to.address = from.address; });

  share(cfg.probability.surname, cfg.coverage.surname,
        [](const auto &from, auto &to) {
          to.name.lastIdx = from.name.lastIdx;
          to.name.middleIdx = from.name.middleIdx;
        });
}

} // namespace

RingPiiCorrelator::RingPiiCorrelator(const entity::person::Topology &topology,
                                     const Sharing &config) noexcept
    : topology_(topology), config_(config) {}

void RingPiiCorrelator::apply(random::Rng &rng,
                              entity::pii::Roster &roster) const {
  if (!config_.enabled) {
    return;
  }
  primitives::validate::require(config_);

  for (const auto &ring : topology_.rings) {
    const auto members =
        correlatableMembers(topology_, ring, config_.includeVictims);
    if (members.size() >= config_.limits.minMembers) {
      correlateRing(rng, config_, roster, members);
    }
  }
}

void correlateRingPii(random::Rng &rng,
                      const entity::person::Topology &topology,
                      const Sharing &config, entity::pii::Roster &roster) {
  RingPiiCorrelator{topology, config}.apply(rng, roster);
}

} // namespace PhantomLedger::synth::pii
