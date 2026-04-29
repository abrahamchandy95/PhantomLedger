#pragma once

#include "phantomledger/relationships/family/network.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::relationships::family {

struct BuildInputs {
  std::span<const ::PhantomLedger::personas::Type> personas;

  std::uint32_t personCount = 0;

  std::uint64_t baseSeed = 0;
};

[[nodiscard]] Graph build(const transfers::family::GraphConfig &cfg,
                          const BuildInputs &inputs);

} // namespace PhantomLedger::relationships::family
