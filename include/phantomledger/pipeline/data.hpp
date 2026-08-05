#pragma once

#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/holdings/cards.hpp"
#include "phantomledger/entities/counterparties/directory.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/parties/pii.hpp"
#include "phantomledger/entities/parties/relocation.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/synth/accounts/pack.hpp"
#include "phantomledger/synth/landlords/pack.hpp"
#include "phantomledger/synth/people/pack.hpp"
#include "phantomledger/synth/personas/pack.hpp"

#include <vector>

namespace PhantomLedger::pipeline {

struct People {
  synth::people::Pack roster;
  entity::pii::Roster pii;
  synth::personas::Pack personas;

  // geo-causal-v1 (G2a): a COMPACT per-person home area (PersonId-1
  // indexed), snapshotted from `pii` at entity-build time. This survives
  // releaseExportOnlyPacks() (which nulls `pii` before the transfer fold),
  // so the causal card-present selection inside the fold can read a
  // customer's home WITHOUT the released PII roster. Populated now
  // (foundation); consumed by the fold once the carrier is threaded to the
  // Spender (G2a step-1 remainder → step-2 selection).
  std::vector<entity::geography::GeoAreaId> homeAreas;

  // relocation-2026-07: the home-area HISTORY. `homeAreas` above keeps its
  // meaning — home at WINDOW START — and that is deliberate: leaving the
  // compact carrier's type and contents alone is what bounds this round's
  // blast radius, since it is threaded through six files and two engines.
  // Consumers that need a point-in-time home read this instead.
  //
  // Survives `releaseExportOnlyPacks()` for the same reason `homeAreas` does:
  // the transfer fold needs it after `pii` is nulled.
  entity::parties::relocation::Schedule relocation;
};

// Snapshot the per-person home area from a fresh PII roster into the
// compact carrier above. Called at entity-build time (PII still alive).
[[nodiscard]] inline std::vector<entity::geography::GeoAreaId>
homeAreasOf(const entity::pii::Roster &pii) {
  std::vector<entity::geography::GeoAreaId> out;
  out.reserve(pii.records.size());
  for (const auto &record : pii.records) {
    out.push_back(record.address.geoArea);
  }
  return out;
}

// pipeline/holdings.hpp
struct Holdings {
  synth::accounts::Pack accounts;
  entity::card::Registry creditCards;
  entity::product::PortfolioRegistry portfolios;
};

// pipeline/counterparties.hpp
struct Counterparties {
  entity::merchant::Catalog merchants;
  synth::landlords::Pack landlords;
  entity::counterparty::Directory counterparties;
};

} // namespace PhantomLedger::pipeline
