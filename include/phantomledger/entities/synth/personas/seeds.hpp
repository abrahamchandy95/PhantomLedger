#pragma once

#include "phantomledger/entities/identifier/key.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::personas {

[[nodiscard]] inline std::uint64_t seed(std::uint64_t base,
                                        entity::PersonId person) noexcept {
  return static_cast<std::uint64_t>(PhantomLedger::hashing::make(
      base, static_cast<std::uint32_t>(person), 0x50455253U));
}

} // namespace PhantomLedger::entities::synth::personas
