#pragma once

#include "phantomledger/taxonomies/enums.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace PhantomLedger::landlords {

using namespace ::PhantomLedger::taxonomies::enums;

enum class Type : std::uint8_t {
  individual = 0,
  llcSmall = 1,
  corporate = 2,
};

inline constexpr auto kTypes = std::to_array<Type>({
    Type::individual,
    Type::llcSmall,
    Type::corporate,
});

inline constexpr std::size_t kTypeCount = kTypes.size();

static_assert(isIndexable(kTypes));

} // namespace PhantomLedger::landlords
