#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::family {

/// Drives student-dependent and parent-link assignment.
struct Dependents {
  double studentDependentP = 0.65;
  double studentCoresidesP = 0.35;
  double twoParentP = 0.70;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check(
        [&] { v::between("studentDependentP", studentDependentP, 0.0, 1.0); });
    r.check(
        [&] { v::between("studentCoresidesP", studentCoresidesP, 0.0, 1.0); });
    r.check([&] { v::between("twoParentP", twoParentP, 0.0, 1.0); });
  }
};

inline constexpr Dependents kDefaultDependents{};

struct Links {
  std::vector<entity::PersonId> spouseOf;

  std::vector<std::array<entity::PersonId, 2>> parentsOf;

  std::vector<std::vector<entity::PersonId>> childrenOf;
};

struct LinkInputs {
  const Households *households = nullptr;
  const Dependents *dependents = nullptr;
  const Partition *partition = nullptr;
  std::span<const ::PhantomLedger::personas::Type> personas;
  std::uint32_t personCount = 0;
};

void assignSpouses(const LinkInputs &inputs, random::Rng &rng, Links &out);

void assignParents(const LinkInputs &inputs, random::Rng &rng, Links &out);

[[nodiscard]] Links buildLinks(const LinkInputs &inputs, random::Rng &rng);

} // namespace PhantomLedger::relationships::family
