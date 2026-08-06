#pragma once

#include "phantomledger/entities/counterparties/directory.hpp"
#include "phantomledger/entities/counterparties/merchants.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/holdings/cards.hpp"
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

  /* A COMPACT per-person home area (PersonId-1 indexed), snapshotted from
   * `pii` at entity-build time. It survives releaseExportOnlyPacks(), which
   * nulls `pii` before the transfer fold, so causal card-present selection
   * inside the fold can read a customer's home WITHOUT the released PII
   * roster. */
  std::vector<entity::geography::GeoAreaId> homeAreas;

  /* The home-area HISTORY. `homeAreas` above deliberately keeps its meaning —
   * home at WINDOW START — because leaving that compact carrier's type and
   * contents alone is what bounds the blast radius of a home-area change; it
   * is threaded through six files and two engines. Consumers that need a
   * point-in-time home read this instead.
   *
   * Survives releaseExportOnlyPacks() for the same reason `homeAreas` does:
   * the transfer fold needs it after `pii` is nulled. */
  entity::parties::relocation::Schedule relocation;
};

/* Snapshot the per-person home area from a fresh PII roster into the compact
 * carrier above. Called at entity-build time, while PII is still alive. */
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
