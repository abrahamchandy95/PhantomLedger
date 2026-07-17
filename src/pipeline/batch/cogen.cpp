#include "phantomledger/pipeline/batch/cogen.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace PhantomLedger::pipeline::batch {

namespace {

[[nodiscard]] constexpr std::size_t histBucket(std::uint32_t size) noexcept {
  const auto width = static_cast<std::size_t>(std::bit_width(size));
  return std::min(width - 1, Stats::kHistBuckets - 1);
}

} // namespace

std::vector<WorkShard> contiguousShards(std::uint32_t personCount,
                                        std::uint32_t workerCount) {
  if (workerCount == 0) {
    throw std::invalid_argument(
        "batch::contiguousShards: workerCount must be positive");
  }

  if (personCount == 0) {
    return {};
  }

  const auto shardCount = std::min(personCount, workerCount);
  const auto baseSize = personCount / shardCount;
  const auto remainder = personCount % shardCount;

  std::vector<WorkShard> shards;
  shards.reserve(shardCount);

  std::uint32_t begin = 0;

  for (std::uint32_t index = 0; index < shardCount; ++index) {
    const auto size = baseSize + static_cast<std::uint32_t>(index < remainder);

    const auto end = begin + size;

    shards.push_back(WorkShard{
        .begin = begin,
        .end = end,
    });

    begin = end;
  }

  return shards;
}

CogenGraph::CogenGraph(std::uint32_t personCount)
    : parent_(personCount), rank_(personCount, 0), min_(personCount) {
  for (std::uint32_t index = 0; index < personCount; ++index) {
    parent_[index] = index;
    min_[index] = static_cast<entity::PersonId>(index + 1);
  }
}

void CogenGraph::requireId(entity::PersonId person, const char *fn) const {
  if (!entity::valid(person) || person > parent_.size()) {
    throw std::out_of_range(std::string{"batch::CogenGraph::"} + fn +
                            ": PersonId " + std::to_string(person) +
                            " outside [1, " + std::to_string(parent_.size()) +
                            "]");
  }
}

std::uint32_t CogenGraph::rootOf(std::uint32_t index) noexcept {
  while (parent_[index] != index) {
    parent_[index] = parent_[parent_[index]];
    index = parent_[index];
  }

  return index;
}

void CogenGraph::link(entity::PersonId lhs, entity::PersonId rhs) {
  requireId(lhs, "link");
  requireId(rhs, "link");

  auto lhsRoot = rootOf(lhs - 1);
  auto rhsRoot = rootOf(rhs - 1);

  if (lhsRoot == rhsRoot) {
    return;
  }

  if (rank_[lhsRoot] < rank_[rhsRoot]) {
    std::swap(lhsRoot, rhsRoot);
  }

  parent_[rhsRoot] = lhsRoot;
  min_[lhsRoot] = std::min(min_[lhsRoot], min_[rhsRoot]);

  if (rank_[lhsRoot] == rank_[rhsRoot]) {
    ++rank_[lhsRoot];
  }
}

void CogenGraph::linkAll(std::span<const entity::PersonId> members) {
  if (members.empty()) {
    return;
  }

  const auto first = members.front();
  requireId(first, "linkAll");

  for (std::size_t index = 1; index < members.size(); ++index) {
    link(first, members[index]);
  }
}

entity::PersonId CogenGraph::representativeOf(entity::PersonId person) {
  requireId(person, "representativeOf");
  return min_[rootOf(person - 1)];
}

void Strategy::validate(primitives::validate::Report &report) const {
  namespace validate = primitives::validate;

  report.check([&] {
    validate::positive("batch.targetShardPeople", targetShardPeople);
  });
}

std::vector<entity::PersonId> Shard::collectPeople() const {
  std::vector<entity::PersonId> people;
  people.reserve(peopleCount);

  for (const auto &component : components) {
    people.insert(people.end(), component.members.begin(),
                  component.members.end());
  }

  std::sort(people.begin(), people.end());
  return people;
}

Partition Partition::build(CogenGraph &graph, const Strategy &strategy) {
  primitives::validate::require(strategy);

  Partition result;

  const auto personCount = graph.personCount();
  result.stats_.personCount = personCount;

  if (personCount == 0) {
    return result;
  }

  constexpr auto kUnseen = std::numeric_limits<std::uint32_t>::max();

  std::vector<std::uint32_t> componentOfRoot(personCount, kUnseen);
  std::vector<Component> components;

  for (std::uint32_t index = 0; index < personCount; ++index) {
    const auto root = graph.rootOf(index);
    auto slot = componentOfRoot[root];

    if (slot == kUnseen) {
      slot = static_cast<std::uint32_t>(components.size());
      componentOfRoot[root] = slot;

      components.push_back(Component{
          .anchor = static_cast<entity::PersonId>(index + 1),
          .members = {},
      });
    }

    components[slot].members.push_back(
        static_cast<entity::PersonId>(index + 1));
  }

  auto &stats = result.stats_;
  stats.componentCount = static_cast<std::uint32_t>(components.size());

  for (const auto &component : components) {
    const auto size = component.size();

    if (size == 1) {
      ++stats.singletonCount;
    }

    if (size > stats.maxComponentSize) {
      stats.maxComponentSize = size;
      stats.maxComponentAnchor = component.anchor;
    }

    ++stats.sizeHistogramLog2[histBucket(size)];
  }

  if (strategy.maxComponentPeople > 0 &&
      stats.maxComponentSize > strategy.maxComponentPeople) {
    throw std::runtime_error(
        "batch::Partition: component anchored at PersonId " +
        std::to_string(stats.maxComponentAnchor) + " has " +
        std::to_string(stats.maxComponentSize) +
        " people, exceeding maxComponentPeople=" +
        std::to_string(strategy.maxComponentPeople) +
        "; world sharding cannot split a planned component -- "
        "reduce the planned coupling or raise the cap");
  }

  Shard current;

  const auto closeShard = [&] {
    if (current.components.empty()) {
      return;
    }

    current.index = static_cast<std::uint32_t>(result.shards_.size());

    result.shards_.push_back(std::move(current));
    current = Shard{};
  };

  for (auto &component : components) {
    const auto size = component.size();

    if (current.peopleCount > 0 &&
        current.peopleCount + size > strategy.targetShardPeople) {
      closeShard();
    }

    current.peopleCount += size;
    current.components.push_back(std::move(component));
  }

  closeShard();

  return result;
}

} // namespace PhantomLedger::pipeline::batch
