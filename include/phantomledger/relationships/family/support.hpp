#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/relationships/family/links.hpp"
#include "phantomledger/relationships/family/partition.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace PhantomLedger::relationships::family {

/// Drives cross-household financial-support ties for retired people.
struct RetireeSupport {
  bool enabled = true;

  double hasChildP = 0.35;
  double supportP = 0.35;
  double coresidesP = 0.12;

  double paretoXm = 25.0;
  double paretoAlpha = 2.4;

  void validate(primitives::validate::Report &r) const {
    namespace v = primitives::validate;
    r.check([&] { v::between("hasChildP", hasChildP, 0.0, 1.0); });
    r.check([&] { v::between("supportP", supportP, 0.0, 1.0); });
    r.check([&] { v::between("coresidesP", coresidesP, 0.0, 1.0); });
    r.check([&] { v::gt("paretoXm", paretoXm, 0.0); });
    r.check([&] { v::gt("paretoAlpha", paretoAlpha, 0.0); });
  }
};

inline constexpr RetireeSupport kDefaultRetireeSupport{};

struct SupportTies {
  std::vector<std::vector<entity::PersonId>> supportingChildrenOf;

  std::vector<std::vector<entity::PersonId>> supportedParentsBy;
};

struct SupportInputs {
  const RetireeSupport *support = nullptr;
  const Partition *partition = nullptr;
  const Links *links = nullptr;
  std::span<const ::PhantomLedger::personas::Type> personas;
  std::uint32_t personCount = 0;
};

[[nodiscard]] SupportTies buildSupportTies(const SupportInputs &inputs,
                                           random::Rng &rng);

} // namespace PhantomLedger::relationships::family
