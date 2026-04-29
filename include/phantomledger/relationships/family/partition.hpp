#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::relationships::family {

struct Partition {
  std::vector<entity::PersonId> members;
  std::vector<std::uint32_t> offsets;
  std::vector<std::uint32_t> householdOf;

  [[nodiscard]] std::uint32_t householdCount() const noexcept {
    return offsets.empty() ? 0U
                           : static_cast<std::uint32_t>(offsets.size() - 1);
  }
};

[[nodiscard]] Partition partition(const transfers::family::Households &cfg,
                                  random::Rng &rng, std::uint32_t personCount);

} // namespace PhantomLedger::relationships::family
