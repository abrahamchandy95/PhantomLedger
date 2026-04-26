#pragma once

#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/spending/market/commerce/view.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>

namespace PhantomLedger::spending::actors {

struct Spender {
  // Identity
  entity::PersonId person = entity::invalidPerson;
  std::uint32_t personIndex = 0;

  // Routing destinations
  entity::Key depositAccount{};
  entity::Key card{};
  bool hasCard = false;

  // Persona
  personas::Type personaType{};
  const entity::behavior::Persona *persona = nullptr;

  std::uint16_t favCount = 0;
  std::uint16_t billCount = 0;
  float exploreProp = 0.0f;
  std::uint32_t burstStart = market::commerce::kNoBurstDay;
  std::uint16_t burstLen = 0;
};

} // namespace PhantomLedger::spending::actors
