#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/entities/infra/shared.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/synth/infra/devices_output.hpp"
#include "phantomledger/synth/infra/ips_output.hpp"
#include "phantomledger/synth/infra/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::pipeline {

namespace clearing = ::PhantomLedger::clearing;
namespace entity = ::PhantomLedger::entity;
namespace entities = ::PhantomLedger::entities;
namespace synth = ::PhantomLedger::synth;
namespace txns = ::PhantomLedger::transactions;
namespace transfers = ::PhantomLedger::transfers;

struct Entities {
  entities::synth::people::Pack people;
  entities::synth::accounts::Pack accounts;
  entity::pii::Roster pii;
  entity::merchant::Catalog merchants;
  entities::synth::landlords::Pack landlords;
  entities::synth::personas::Pack personas;
  entity::card::Registry creditCards;
  entity::counterparty::Directory counterparties;
  entity::product::PortfolioRegistry portfolios;
};

struct Infra {
  std::unordered_map<std::uint32_t, synth::infra::RingPlan> ringPlans;
  synth::infra::devices::Output devices;
  synth::infra::ips::Output ips;

  ::PhantomLedger::infra::Router router;
  ::PhantomLedger::infra::SharedInfra ringInfra;
};

/// Drop accounting emitted by the chronological replay pass.
struct ReplayDrops {
  transfers::legit::ledger::ReplayDropLedger::Counts byReason;
  transfers::legit::ledger::ReplayDropLedger::CountsByChannel byChannel;
};

struct CandidateLedgerReplay {
  std::vector<txns::Transaction> txns;
  ReplayDrops drops;
};

/// Ledger state after fraud injection and the final replay pass.
struct PostedLedgerReplay {
  std::vector<txns::Transaction> txns;
  std::unique_ptr<clearing::Ledger> book;
};

/// Transfer ledger artifacts produced by the transfer stage.
struct TransferLedger {
  CandidateLedgerReplay candidate;
  PostedLedgerReplay posted;
};

struct FraudInjectionSummary {
  std::size_t injectedCount = 0;
};

struct Transfers {
  transfers::legit::ledger::LegitTransferResult legit;
  FraudInjectionSummary fraud;
  TransferLedger ledger;
};

} // namespace PhantomLedger::pipeline
