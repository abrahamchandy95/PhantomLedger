#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/infra/devices.hpp"
#include "phantomledger/entities/infra/ipv4.hpp"
#include "phantomledger/entities/infra/tenure.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/*
  The attacker-side access layer: exogenous fraud infrastructure that PERSISTS
  AND IS REUSED ACROSS VICTIMS.

  "One device touching many cards" is the single most valuable signal a
  card-fraud graph carries — it is the reason to model this as a graph at all.
  Minting a fresh device and IP per compromise makes cross-victim endpoint
  sharing ZERO BY CONSTRUCTION: no message can pass between two victims
  through an endpoint that only ever sees one of them, and the Device and IP
  layers go inert for a GNN no matter how they are exported.

  AN OPERATOR IS A CAMPAIGN, NOT A PERSON. Real card fraud monetizes stolen
  credentials through infrastructure with a LIFETIME: devices/fingerprints and
  addresses worked for weeks or months across many cards, then burned and
  replaced. An `AttackerOperator` is a bounded campaign holding concurrent
  device and IP LINES, each line a replacement chain that TILES the campaign
  the way `synth::infra::timeline::sampleChain` tiles a customer's window.

  OPERATORS ARE DELIBERATELY NOT ROSTER PARTIES. The attacker population is
  exogenous and far larger than any plausible in-corpus fraud cohort; making
  them customers would trade a missing signal for a false one. What they do
  have is internal structure, which is all the graph needed.

  EVERY LOOKUP HERE MUST STAY DRAW-FREE AND STATELESS. The fraud planner
  resolves a case's endpoints through these methods:
    * DRAW-FREE holds the planner's per-plan RNG footprint at the four draws
      `network::randomIpv4` spent, which is what confines golden movement to
      the device/IP columns instead of re-rolling every amount, timestamp and
      row count.
    * STATELESS keeps the batch and windowed engines in lockstep. The Router's
      sticky index is legitimate for customers, whose sessions are a walk; an
      attacker case is a point query, and a mutable cache here would make the
      answer depend on injection order.

  Selection among CONCURRENTLY-live endpoints is therefore keyed on a
  caller-supplied salt (the plan sequence), never on a coin. Two cases run by
  one operator at the same instant may land on different lines — a botnet is
  several machines — while one case keeps one endpoint for all of its events,
  which is what "a case has one modus operandi" means.
 */

namespace PhantomLedger::infra {

// The internal `devices::Identity::ownerId` base for attacker
// infrastructure. It stays an INTERNAL discriminator only: every
// assigned identity renders through one opaque, fixed-width `D…`
// namespace (exporter/common/render.hpp), so neither this value nor
// `OwnerType::ring` is observable downstream. `slot` now genuinely
// varies — it indexes the operator's own endpoint inventory.
inline constexpr std::uint64_t kAttackerOwnerIdBase = 0xACE00000ULL;

struct AttackerOperator {
  // Half-open campaign horizon. Outside it the operator holds nothing.
  std::int64_t campaignFirstEpoch = 0;
  std::int64_t campaignLastEpochExcl = 0;

  // Half-open index ranges into the flat endpoint arrays below.
  std::uint32_t deviceBegin = 0;
  std::uint32_t deviceEnd = 0;
  std::uint32_t ipBegin = 0;
  std::uint32_t ipEnd = 0;

  // Case-load weight. Heavy-tailed on purpose: a handful of operators
  // work many cards and most work a few, which is what produces a
  // realistic victims-per-endpoint degree distribution rather than a
  // flat one.
  double weight = 1.0;

  [[nodiscard]] constexpr bool liveAt(std::int64_t ts) const noexcept {
    return ts >= campaignFirstEpoch && ts < campaignLastEpochExcl;
  }
};

class AttackerInfra {
public:
  // ---------------------------------------------------------------
  // Storage. Flat and parallel: `deviceTenures[i]` is the interval
  // during which `devices[i]` was in service, exactly the contract
  // `synth::infra::devices::Output::tenureByPerson` keeps for
  // customers. Lines are emitted CONTIGUOUSLY per operator, so an
  // operator's range is a concatenation of tiling chains.
  std::vector<AttackerOperator> operators;

  std::vector<devices::Identity> devices;
  std::vector<Tenure> deviceTenures;

  std::vector<network::Ipv4> ips;
  std::vector<Tenure> ipTenures;

  // Membership lookups for the exporter's ground-truth overlay. Sorted
  // copies rather than hash sets: the sets are small, the comparison is
  // total, and a sorted vector cannot reorder between runs.
  std::vector<devices::Identity> sortedDevices;
  std::vector<network::Ipv4> sortedIps;

  /* THE COMPROMISED-HOST POOL: roster people whose OWN machines operators
   * run cases from. Sorted and unique, so membership is a binary search and
   * selection is an index.
   *
   * IT IS A POOL AND NOT A PER-CASE PICK, AND THAT IS THE WHOLE POINT. A
   * host drawn afresh for every case serves one victim, so the machine it
   * lends carries exactly one compromise and cannot pass a message between
   * two victims — the same "zero by construction" this file exists to
   * remove, one layer down. A bounded pool makes one consumer device carry
   * fraud against SEVERAL victims while it keeps carrying its owner's
   * ordinary purchases, which is the only population in the model that is
   * high-degree, mixed, and attacker-operated at once.
   *
   * THESE PEOPLE'S DEVICES ARE DELIBERATELY NOT IN `sortedDevices`. A host
   * is a victim of a second crime, not attacker inventory: its identity
   * belongs to a customer, it is on that customer's `Has_Device` file, and
   * its tenure is the customer's. Marking it attacker-owned would flip the
   * exporter's entity verdict on a machine whose traffic is overwhelmingly
   * legitimate — the same call `export.cpp` already makes for a
   * residential-proxy address, for the same reason. `runsFrom` is the
   * separate predicate, and it is what makes attacker-operated purity a
   * FALSIFIABLE quantity rather than one that reads zero by construction.
   *
   * DERIVED DRAW-FREE FROM THE ROSTER SIZE at build time, so it is world
   * state in both engines and costs the attacker lane nothing. */
  std::vector<entity::PersonId> compromisedHosts;

  // Coarse time index over the campaign span, so selecting among live
  // operators does not scan the whole population per case. An operator
  // appears in every bucket its campaign overlaps.
  std::int64_t bucketFirstEpoch = 0;
  std::int64_t bucketSeconds = 30LL * 86'400LL;
  std::vector<std::uint32_t> bucketOffsets;   // size = buckets + 1
  std::vector<std::uint32_t> bucketOperators; // CSR payload

  [[nodiscard]] bool empty() const noexcept { return operators.empty(); }

  [[nodiscard]] std::size_t operatorCount() const noexcept {
    return operators.size();
  }
  [[nodiscard]] std::size_t deviceCount() const noexcept {
    return devices.size();
  }
  [[nodiscard]] std::size_t ipCount() const noexcept { return ips.size(); }

  /* Build the membership and time indexes. Call once, after the
   * generator has filled the arrays above. */
  void index() {
    sortedDevices = devices;
    std::sort(sortedDevices.begin(), sortedDevices.end());
    sortedDevices.erase(std::unique(sortedDevices.begin(), sortedDevices.end()),
                        sortedDevices.end());

    sortedIps = ips;
    std::sort(sortedIps.begin(), sortedIps.end());
    sortedIps.erase(std::unique(sortedIps.begin(), sortedIps.end()),
                    sortedIps.end());

    bucketOffsets.clear();
    bucketOperators.clear();
    if (operators.empty()) {
      return;
    }

    std::int64_t first = operators.front().campaignFirstEpoch;
    std::int64_t lastExcl = operators.front().campaignLastEpochExcl;
    for (const auto &op : operators) {
      first = std::min(first, op.campaignFirstEpoch);
      lastExcl = std::max(lastExcl, op.campaignLastEpochExcl);
    }
    bucketFirstEpoch = first;

    const auto span = std::max<std::int64_t>(1, lastExcl - first);
    const auto buckets =
        static_cast<std::size_t>((span + bucketSeconds - 1) / bucketSeconds);

    std::vector<std::uint32_t> counts(buckets, 0);
    const auto bucketRange = [&](const AttackerOperator &op) {
      const auto lo = static_cast<std::size_t>(
          std::max<std::int64_t>(0, op.campaignFirstEpoch - first) /
          bucketSeconds);
      const auto hiRaw = static_cast<std::size_t>(
          std::max<std::int64_t>(0, op.campaignLastEpochExcl - 1 - first) /
          bucketSeconds);
      return std::pair<std::size_t, std::size_t>{std::min(lo, buckets - 1),
                                                 std::min(hiRaw, buckets - 1)};
    };

    for (const auto &op : operators) {
      const auto [lo, hi] = bucketRange(op);
      for (auto b = lo; b <= hi; ++b) {
        ++counts[b];
      }
    }

    bucketOffsets.assign(buckets + 1, 0);
    for (std::size_t b = 0; b < buckets; ++b) {
      bucketOffsets[b + 1] = bucketOffsets[b] + counts[b];
    }
    bucketOperators.assign(bucketOffsets.back(), 0);

    std::vector<std::uint32_t> cursor(bucketOffsets.begin(),
                                      bucketOffsets.end() - 1);
    for (std::size_t i = 0; i < operators.size(); ++i) {
      const auto [lo, hi] = bucketRange(operators[i]);
      for (auto b = lo; b <= hi; ++b) {
        bucketOperators[cursor[b]++] = static_cast<std::uint32_t>(i);
      }
    }
  }

  [[nodiscard]] bool ownsDevice(devices::Identity id) const noexcept {
    return std::binary_search(sortedDevices.begin(), sortedDevices.end(), id);
  }

  [[nodiscard]] bool ownsIp(network::Ipv4 address) const noexcept {
    return std::binary_search(sortedIps.begin(), sortedIps.end(), address);
  }

  /* Whether operators run cases from this person's machines. DISTINCT FROM
   * `ownsDevice` on purpose — see `compromisedHosts`. A caller measuring
   * "attacker-operated endpoints" wants the UNION; a caller writing an
   * entity verdict wants `ownsDevice` alone. */
  [[nodiscard]] bool runsFrom(entity::PersonId person) const noexcept {
    return std::binary_search(compromisedHosts.begin(), compromisedHosts.end(),
                              person);
  }

  /* The host a case runs from, chosen by `salt` alone. Draw-free and
   * stateless, like every other selector here: the caller owns the entropy
   * and nothing is written back.
   *
   * `salt` is the plan sequence, so successive cases spread over the pool
   * while one case keeps one host for all of its events. */
  [[nodiscard]] std::optional<entity::PersonId>
  hostAt(std::uint64_t salt) const noexcept {
    if (compromisedHosts.empty()) {
      return std::nullopt;
    }
    return compromisedHosts[static_cast<std::size_t>(salt %
                                                     compromisedHosts.size())];
  }

  /* Weighted pick among the operators whose campaign contains `ts`.
   * `u` must be in [0, 1). Draw-free: the caller owns the uniform. */
  [[nodiscard]] std::optional<std::uint32_t> operatorAt(double u,
                                                        std::int64_t ts) const {
    if (bucketOffsets.size() < 2) {
      return std::nullopt;
    }
    const auto buckets = bucketOffsets.size() - 1;
    if (ts < bucketFirstEpoch) {
      return std::nullopt;
    }
    const auto raw =
        static_cast<std::size_t>((ts - bucketFirstEpoch) / bucketSeconds);
    if (raw >= buckets) {
      return std::nullopt;
    }

    // Two passes over one small bucket: total live weight, then the
    // weighted position. No allocation on the hot path.
    double total = 0.0;
    for (auto k = bucketOffsets[raw]; k < bucketOffsets[raw + 1]; ++k) {
      const auto &op = operators[bucketOperators[k]];
      if (op.liveAt(ts)) {
        total += op.weight;
      }
    }
    if (!(total > 0.0)) {
      return std::nullopt;
    }

    double cursor = u * total;
    std::uint32_t last = 0;
    bool any = false;
    for (auto k = bucketOffsets[raw]; k < bucketOffsets[raw + 1]; ++k) {
      const auto idx = bucketOperators[k];
      const auto &op = operators[idx];
      if (!op.liveAt(ts)) {
        continue;
      }
      any = true;
      last = idx;
      cursor -= op.weight;
      if (cursor < 0.0) {
        return idx;
      }
    }
    // Floating-point residue only; the loop above already proved the
    // live set non-empty.
    return any ? std::optional<std::uint32_t>{last} : std::nullopt;
  }

  /* An endpoint the operator held for the WHOLE of [`ts`, `endTsExcl`).
   *
   * The whole-span requirement is what makes the exported session
   * point-in-time honest with ZERO exceptions. A compromise keeps ONE
   * endpoint for all of its events — a case has one modus operandi —
   * and its events run up to 71 hours past the case date, so an
   * endpoint chosen on liveness at the case START alone could be
   * attributed to an event after it had already been replaced. That is
   * exactly the class of contradiction measured at 53% on the customer
   * endpoint side, and admitting a few percent of it here would be
   * reintroducing it in miniature.
   *
   * Returning nullopt is a legitimate answer: no single endpoint
   * covered the case, so this operator did not run it. The caller falls
   * back to victim-endpoint attribution, which is a modelled mechanism
   * rather than a patch, and the realized share is gated. */
  [[nodiscard]] std::optional<devices::Identity>
  deviceAt(std::uint32_t operatorIndex, std::int64_t ts, std::int64_t endTsExcl,
           std::uint64_t salt) const {
    const auto pick =
        coveringSpan(operatorIndex, ts, endTsExcl, salt, deviceTenures,
                     [&](const AttackerOperator &op) {
                       return std::pair{op.deviceBegin, op.deviceEnd};
                     });
    if (!pick.has_value()) {
      return std::nullopt;
    }
    return devices[*pick];
  }

  [[nodiscard]] std::optional<network::Ipv4> ipAt(std::uint32_t operatorIndex,
                                                  std::int64_t ts,
                                                  std::int64_t endTsExcl,
                                                  std::uint64_t salt) const {
    const auto pick = coveringSpan(operatorIndex, ts, endTsExcl, salt,
                                   ipTenures, [&](const AttackerOperator &op) {
                                     return std::pair{op.ipBegin, op.ipEnd};
                                   });
    if (!pick.has_value()) {
      return std::nullopt;
    }
    return ips[*pick];
  }

private:
  // Pick the salt-th entry in an operator's range whose tenure spans the
  // whole half-open case interval. Chains TILE the campaign, so within
  // the campaign the count of merely-live entries equals the number of
  // concurrent LINES; requiring whole-span coverage thins that set near
  // a replacement boundary and can empty it, which the caller handles.
  template <typename RangeOf>
  [[nodiscard]] std::optional<std::size_t>
  coveringSpan(std::uint32_t operatorIndex, std::int64_t ts,
               std::int64_t endTsExcl, std::uint64_t salt,
               const std::vector<Tenure> &tenures, RangeOf rangeOf) const {
    if (operatorIndex >= operators.size()) {
      return std::nullopt;
    }
    const auto &op = operators[operatorIndex];
    const auto [begin, end] = rangeOf(op);
    if (begin >= end || end > tenures.size()) {
      return std::nullopt;
    }

    const auto covers = [&](std::size_t i) {
      return tenures[i].contains(ts) && tenures[i].lastEpochExcl >= endTsExcl;
    };

    std::size_t live = 0;
    for (auto i = begin; i < end; ++i) {
      if (covers(i)) {
        ++live;
      }
    }
    if (live == 0) {
      return std::nullopt;
    }

    auto want = static_cast<std::size_t>(salt % live);
    for (auto i = begin; i < end; ++i) {
      if (!covers(i)) {
        continue;
      }
      if (want == 0) {
        return static_cast<std::size_t>(i);
      }
      --want;
    }
    return std::nullopt;
  }
};

} // namespace PhantomLedger::infra
