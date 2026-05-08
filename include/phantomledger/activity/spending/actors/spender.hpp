#pragma once

#include "phantomledger/activity/spending/market/commerce/view.hpp"
#include "phantomledger/entities/behaviors.hpp"
#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/taxonomies/personas/types.hpp"

#include <cstdint>
#include <limits>

namespace PhantomLedger::spending::actors {

/// Per-person data needed by the spending hot loop.

struct Spender {

  static constexpr std::uint32_t kInvalidLedgerIndex =
      std::numeric_limits<std::uint32_t>::max();

  // Identity
  entity::PersonId person = entity::invalidPerson;
  std::uint32_t personIndex = 0;

  // Routing destinations (Keys + their resolved Ledger indices).
  entity::Key depositAccount{};
  entity::Key card{};
  std::uint32_t depositAccountIdx = kInvalidLedgerIndex;
  std::uint32_t cardIdx = kInvalidLedgerIndex;
  bool hasCard = false;

  // Persona type + back-pointer for cold fields not cached below.
  personas::Type personaType{};
  const entity::behavior::Persona *persona = nullptr;

  // --- Hot-path persona fields (cached at prepareSpenders time) ---
  double rateMultiplier = 1.0;
  double amountMultiplier = 1.0;
  double cardShare = 0.0;
  personas::Timing timing{};

  // Per-person merchant pool sizes (still derived from CSR row sizes).
  std::uint16_t favCount = 0;
  std::uint16_t billCount = 0;

  float exploreProp = 0.0f;
  std::uint32_t burstStart = market::commerce::kNoBurstDay;
  std::uint16_t burstLen = 0;
};

} // namespace PhantomLedger::spending::actors
