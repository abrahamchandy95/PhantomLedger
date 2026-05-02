#pragma once

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/primitives/hashing/combine.hpp"

#include <cstdint>

namespace PhantomLedger::entities::synth::products {

inline constexpr std::uint64_t kDefaultProductsSeed = 0xB0A7F00DULL;

[[nodiscard]] inline ::PhantomLedger::random::Rng
personRng(std::uint64_t baseSeed, ::PhantomLedger::entity::PersonId person) {
  constexpr std::uint64_t kPortfolioSalt = 0xD0E550F150F1C0DEULL;
  const auto seed = ::PhantomLedger::hashing::make(
      baseSeed, kPortfolioSalt, static_cast<std::uint64_t>(person));

  return ::PhantomLedger::random::Rng::fromSeed(seed);
}

} // namespace PhantomLedger::entities::synth::products
