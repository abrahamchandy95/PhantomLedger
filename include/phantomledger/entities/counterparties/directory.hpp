#pragma once

#include "phantomledger/entities/counterparties/sized_pool.hpp"
#include "phantomledger/entities/geography/area.hpp"
#include "phantomledger/entities/identifiers.hpp"

#include <vector>

namespace PhantomLedger::entity::counterparty {

struct BankSplit {
  std::vector<entity::Key> internal;
  std::vector<entity::Key> external;
  std::vector<entity::Key> all;
};

struct Employers {
  BankSplit accounts;
  // The size law over accounts.external, in roster order
  // (counterparty-sizes-2026-09). Employers are all external.
  SizedPool pool;
};

struct ClientPayers {
  BankSplit accounts;
};

struct ExternalParties {
  std::vector<entity::Key> platforms;
  std::vector<entity::Key> processors;
  std::vector<entity::Key> ownerBusinesses;
  std::vector<entity::Key> brokerages;

  // Observable cash-service locations. They are registered counterparties,
  // never customer-owned or balance-bearing ledger accounts.
  std::vector<entity::Key> atmTerminals;
  std::vector<entity::geography::GeoAreaId> atmTerminalAreas;
  std::vector<entity::Key> cashDepositories;
  std::vector<entity::geography::GeoAreaId> cashDepositoryAreas;
  std::vector<entity::Key> checkCapturePoints;
  std::vector<entity::geography::GeoAreaId> checkCaptureAreas;

  // Bank-visible fiat ramps at crypto service providers. Native-token wallet
  // activity remains outside this USD customer-ledger projection.
  std::vector<entity::Key> cryptoVenues;

  // Separate service roles that previously fell back to customer accounts.
  std::vector<entity::Key> billers;
};

struct Directory {
  Employers employers;
  ClientPayers clients;
  ExternalParties external;
};

} // namespace PhantomLedger::entity::counterparty
