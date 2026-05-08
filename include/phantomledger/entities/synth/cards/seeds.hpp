#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::cards {

/// Domain-separation constant for the cards subsystem. XORed with the
/// top-level seed to give the cards subsystem its own RNG stream that
/// doesn't collide with people, accounts, pools, transfers, etc.
///
/// This pattern matches the one used by `derivePoolSeed` in main.cpp's
/// pool generation, by `kIssuanceTag` below for per-person derivation,
/// and by other subsystem domain constants throughout the codebase.
inline constexpr std::uint64_t kCardSeedDomain = 0xC0DECAFEULL;

/// Per-person hashing tag for issuance-stage seeds. Different from
/// `kCardSeedDomain` because it applies one level deeper (per-person,
/// not per-subsystem).
inline constexpr std::uint32_t kIssuanceTag = 0x43434953U;

/// Derive the cards-subsystem RNG base from the user's top-level seed.
/// All card issuance ultimately roots in the user-provided `--seed`,
/// passed through this XOR-mixer to keep the cards stream independent
/// of pools, fraud, transfers, etc.
[[nodiscard]] constexpr std::uint64_t
deriveCardSeed(std::uint64_t topLevelSeed) noexcept {
  return topLevelSeed ^ kCardSeedDomain;
}

/// Per-person issuance seed: combines the cards-subsystem base with a
/// person ID via blake2b so each person draws an independent issuance
/// outcome.
[[nodiscard]] inline std::uint64_t
issuanceSeed(std::uint64_t base, entity::PersonId person) noexcept {
  return static_cast<std::uint64_t>(PhantomLedger::hashing::make(
      base, static_cast<std::uint32_t>(person), kIssuanceTag));
}

} // namespace PhantomLedger::entities::synth::cards
