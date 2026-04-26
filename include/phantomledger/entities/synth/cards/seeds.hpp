#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::cards {

inline constexpr std::uint32_t kIssuanceTag = 0x43434953U;

[[nodiscard]] inline std::uint64_t
issuanceSeed(std::uint64_t base, entity::PersonId person) noexcept {
  return static_cast<std::uint64_t>(PhantomLedger::hashing::make(
      base, static_cast<std::uint32_t>(person), kIssuanceTag));
}

} // namespace PhantomLedger::entities::synth::cards
