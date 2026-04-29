#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::family {

struct SupportTies {
  std::vector<std::vector<entity::PersonId>> supportingChildrenOf;

  std::vector<std::vector<entity::PersonId>> supportedParentsBy;
};

struct SupportInputs {
  const transfers::family::RetireeSupport *cfg = nullptr;
  const Partition *partition = nullptr;
  const Links *links = nullptr;
  std::span<const ::PhantomLedger::personas::Type> personas;
  std::uint32_t personCount = 0;
};

[[nodiscard]] SupportTies buildSupportTies(const SupportInputs &inputs,
                                           random::Rng &rng);

} // namespace PhantomLedger::relationships::family
