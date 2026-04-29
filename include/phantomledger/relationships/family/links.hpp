#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::family {

struct Links {
  std::vector<entity::PersonId> spouseOf;

  std::vector<std::array<entity::PersonId, 2>> parentsOf;

  std::vector<std::vector<entity::PersonId>> childrenOf;
};

struct LinkInputs {
  const transfers::family::GraphConfig *cfg = nullptr;
  const Partition *partition = nullptr;
  std::span<const ::PhantomLedger::personas::Type> personas;
  std::uint32_t personCount = 0;
};

void assignSpouses(const LinkInputs &inputs, random::Rng &rng, Links &out);

void assignParents(const LinkInputs &inputs, random::Rng &rng, Links &out);

[[nodiscard]] Links buildLinks(const LinkInputs &inputs, random::Rng &rng);

} // namespace PhantomLedger::relationships::family
