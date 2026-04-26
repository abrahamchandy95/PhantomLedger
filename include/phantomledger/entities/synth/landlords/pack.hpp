#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entities/landlords.hpp"

#include <vector>

namespace PhantomLedger::entities::synth::landlords {

struct Pack {
  entity::landlord::Roster roster;
  entity::landlord::Index index;

  /// Landlords who bank at our institution. Rent payments to these
  /// counterparties settle as internal book-to-book transfers.
  std::vector<entity::Key> internals;

  /// Landlords who bank elsewhere. Rent payments go through
  /// interbank ACH.
  std::vector<entity::Key> externals;
};

} // namespace PhantomLedger::entities::synth::landlords
