#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/merchants/types.hpp"

#include <cstdint>
#include <vector>

namespace PhantomLedger::entity::merchant {

struct Label {
  std::uint64_t value = 0;

  friend constexpr bool operator==(const Label &,
                                   const Label &) noexcept = default;
};

struct Record {
  Label label;
  entity::Key counterpartyId;
  ::PhantomLedger::merchants::Category category;
  double weight = 0.0;
};

struct Catalog {
  std::vector<Record> records;
};

} // namespace PhantomLedger::entity::merchant
