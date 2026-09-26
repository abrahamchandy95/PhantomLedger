#pragma once

#include "phantomledger/entities/counterparties/sized_pool.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/landlords/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PhantomLedger::entity::landlord {

using Type = ::PhantomLedger::landlords::Type;

inline constexpr std::size_t kTypeCount =
    ::PhantomLedger::landlords::kTypeCount;

struct Record {
  entity::Key accountId;
  Type type = Type::individual;
};

// Records are class-contiguous and serial == ordinal + 1 across both banks,
// so `pool` (the size law, counterparty-sizes-2026-09) indexes them directly.
struct Roster {
  std::vector<Record> records;
  counterparty::SizedPool pool;
};

struct Index {
  std::array<std::vector<std::uint32_t>, kTypeCount> byClass;
};

} // namespace PhantomLedger::entity::landlord
