#pragma once

#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/network.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/relationships/family/support.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <span>

namespace PhantomLedger::relationships::family {

struct BuildInputs {
  std::span<const ::PhantomLedger::personas::Type> personas;

  std::uint32_t personCount = 0;

  std::uint64_t baseSeed = 0;
};

[[nodiscard]] Graph
build(const BuildInputs &inputs,
      const Households &households = kDefaultHouseholds,
      const Dependents &dependents = kDefaultDependents,
      const RetireeSupport &retireeSupport = kDefaultRetireeSupport);

} // namespace PhantomLedger::relationships::family
