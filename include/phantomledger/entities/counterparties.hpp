#pragma once

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
};

struct ClientPayers {
  BankSplit accounts;
};

struct ExternalParties {
  std::vector<entity::Key> platforms;
  std::vector<entity::Key> processors;
  std::vector<entity::Key> ownerBusinesses;
  std::vector<entity::Key> brokerages;
};

struct Directory {
  Employers employers;
  ClientPayers clients;
  ExternalParties external;
};

} // namespace PhantomLedger::entity::counterparty
