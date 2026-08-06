#pragma once
/*
  The scaling architecture is CHRONOLOGICAL: one persistent simulation session
  advances the FULL population through sequential generation windows (default
  3 months), with monthly settlement spans flushed and freed behind a lookahead
  watermark. Population is never partitioned across the corpus in v1 (K = 1),
  so this file serves two roles.

  ROLE 1 — WORK SHARDS (output-neutral, used now).
    Within one simulated day, the population is divided into contiguous index
    ranges processed in parallel. Contract: work shards may steer WHERE work
    happens, never WHAT bytes are generated. That holds because per-person
    draws come from person-keyed streams, day-frame/commerce draws happen on
    the single-threaded prefix of the day, and postings synchronize at the day
    boundary. The thread-invariance test — same corpus for 1, 4, 12 workers —
    is the enforcement.

  ROLE 2 — PLANNED-COUPLING GRAPH (diagnostics now; world model later).
    CogenGraph/Partition record which people are coupled BY PLAN (households,
    family links, community blocks, ring rosters, compromise victim/drop
    pairs). In v1 nothing consumes this at generation time; it exists to
    (a) measure planned coupling density, and (b) become the assigner for the
    EXPLICIT K>1 sharded-world model later. In that model K is an
    output-defining, pinned configuration parameter, like population size:
    sampling is restricted per shard BEFORE any generation, shards run as
    independent chronological sessions, and the exported graph is
    block-diagonal by construction. That is a different simulation model, not
    an optimization of this one — DO NOT wire it in until the time-window
    implementation is stable and the K>1 realism trade-offs (WCC/PageRank/path
    features, negative sampling) are explicitly accepted.
 */

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::pipeline::batch {

// ------------------------------------------------------------ work shards

// Contiguous half-open person-INDEX range [begin, end) processed by one
// worker within a day. Index space is 0-based (PersonId - 1).
struct WorkShard {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;

  [[nodiscard]] std::uint32_t size() const noexcept { return end - begin; }
};

// Deterministic balanced split of [0, personCount) into at most
// workerCount contiguous shards (never empty ones). Pure function of its
// arguments; produces identical shard boundaries on every run, so any
// accidental output dependence on sharding shows up as a reproducible
// diff, not a heisenbug. Matches the WorkerPool range semantics so
// thread_runner can adopt it as the single source of truth.
[[nodiscard]] std::vector<WorkShard>
contiguousShards(std::uint32_t personCount, std::uint32_t workerCount);

// --------------------------------------------- planned-coupling graph

// Disjoint-set forest over the 1-based PersonId space [1, personCount].
// Union by rank with path halving, plus min-member tracking so the
// canonical representative (smallest id in the component) resolves after
// edges are fed. Records PLANNED couplings only; draws no randomness.
class CogenGraph {
public:
  explicit CogenGraph(std::uint32_t personCount);

  [[nodiscard]] std::uint32_t personCount() const noexcept {
    return static_cast<std::uint32_t>(parent_.size());
  }

  // Declare that a and b are coupled by plan.
  void link(entity::PersonId a, entity::PersonId b);

  // Clique shorthand: every member coupled with every other.
  void linkAll(std::span<const entity::PersonId> members);

  // Smallest PersonId in p's component (the component's canonical anchor).
  [[nodiscard]] entity::PersonId representativeOf(entity::PersonId p);

private:
  friend class Partition;

  [[nodiscard]] std::uint32_t rootOf(std::uint32_t idx) noexcept;
  void requireId(entity::PersonId p, const char *fn) const;

  std::vector<std::uint32_t> parent_; // index = id - 1
  std::vector<std::uint8_t> rank_;
  std::vector<entity::PersonId> min_; // per root: smallest member id
};

struct Component {
  entity::PersonId anchor = entity::invalidPerson; // smallest member
  std::vector<entity::PersonId> members;           // ascending

  [[nodiscard]] std::uint32_t size() const noexcept {
    return static_cast<std::uint32_t>(members.size());
  }
};

struct Strategy {
  // Packing target for the FUTURE K>1 world model: people per world
  // shard, i.e. ceil(personCount / K). For diagnostics-only use, leave
  // the default; the histogram is what matters.
  std::uint32_t targetShardPeople = 20'000;

  // Hard cap on planned-component size; 0 disables. build() throws if
  // exceeded: an over-connected planned component (e.g. a runaway
  // community) would defeat any future population partition and should
  // be caught at plan time, loudly.
  std::uint32_t maxComponentPeople = 0;

  void validate(primitives::validate::Report &r) const;
};

struct Stats {
  static constexpr std::size_t kHistBuckets = 24;

  std::uint32_t personCount = 0;
  std::uint32_t componentCount = 0;
  std::uint32_t singletonCount = 0;
  std::uint32_t maxComponentSize = 0;
  entity::PersonId maxComponentAnchor = entity::invalidPerson;

  // sizeHistogramLog2[i] counts components with size in [2^i, 2^(i+1)).
  std::array<std::uint32_t, kHistBuckets> sizeHistogramLog2{};
};

struct Shard {
  std::uint32_t index = 0;
  std::uint32_t peopleCount = 0;
  std::vector<Component> components; // anchor-ascending

  // All members across components, ascending.
  [[nodiscard]] std::vector<entity::PersonId> collectPeople() const;
};

// Extracts canonical components and greedily packs them, in anchor order,
// toward targetShardPeople. Pure function of (edge set, Strategy). In the
// future K>1 model this partition is OUTPUT-DEFINING and must run before
// any sampling that it constrains; in v1 it is diagnostics only.
class Partition {
public:
  [[nodiscard]] static Partition build(CogenGraph &graph,
                                       const Strategy &strategy);

  [[nodiscard]] std::span<const Shard> shards() const noexcept {
    return {shards_.data(), shards_.size()};
  }

  [[nodiscard]] const Stats &stats() const noexcept { return stats_; }

  [[nodiscard]] std::size_t size() const noexcept { return shards_.size(); }
  [[nodiscard]] bool empty() const noexcept { return shards_.empty(); }

  [[nodiscard]] const Shard &operator[](std::size_t i) const noexcept {
    return shards_[i];
  }

  [[nodiscard]] auto begin() const noexcept { return shards_.begin(); }
  [[nodiscard]] auto end() const noexcept { return shards_.end(); }

private:
  std::vector<Shard> shards_;
  Stats stats_{};
};

} // namespace PhantomLedger::pipeline::batch
