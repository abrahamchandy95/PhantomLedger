#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::relationships::family {

/// Drives physical residence partitioning.
struct Households {
  double singleP = 0.29;
  double zipfAlpha = 2.2;
  int maxSize = 6;
  double spouseP = 0.62;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("singleP", singleP, 0.0, 1.0); });
    r.check([&] { v::gt("zipfAlpha", zipfAlpha, 0.0); });
    r.check([&] { v::ge("maxSize", maxSize, 2); });
    r.check([&] { v::between("spouseP", spouseP, 0.0, 1.0); });
  }
};

inline constexpr Households kDefaultHouseholds{};

struct Partition {
  std::vector<entity::PersonId> members;
  std::vector<std::uint32_t> offsets;
  std::vector<std::uint32_t> householdOf;

  [[nodiscard]] std::uint32_t householdCount() const noexcept {
    return offsets.empty() ? 0U
                           : static_cast<std::uint32_t>(offsets.size() - 1);
  }
};

[[nodiscard]] Partition partition(const Households &households,
                                  random::Rng &rng, std::uint32_t personCount);

} // namespace PhantomLedger::relationships::family
