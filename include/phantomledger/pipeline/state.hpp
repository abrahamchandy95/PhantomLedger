#pragma once

#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/pii.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/entities/synth/accounts/pack.hpp"
#include "phantomledger/entities/synth/landlords/pack.hpp"
#include "phantomledger/entities/synth/merchants/pack.hpp"
#include "phantomledger/entities/synth/people/pack.hpp"
#include "phantomledger/entities/synth/personas/pack.hpp"
#include "phantomledger/infra/synth/devices_output.hpp"
#include "phantomledger/infra/synth/ips_output.hpp"
#include "phantomledger/infra/synth/types.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/infra/router.hpp"
#include "phantomledger/transactions/infra/shared.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::pipeline {

struct Entities {
  entities::synth::people::Pack people;
  entities::synth::accounts::Pack accounts;
  entity::pii::Roster pii;
  entities::synth::merchants::Pack merchants;
  entities::synth::landlords::Pack landlords;
  entities::synth::personas::Pack personas;
  entity::card::Registry creditCards;
  entity::counterparty::Directory counterparties;
  entity::product::PortfolioRegistry portfolios;
};

struct Infra {
  std::unordered_map<std::uint32_t, infra::synth::RingPlan> ringPlans;
  infra::synth::devices::Output devices;
  infra::synth::ips::Output ips;

  ::PhantomLedger::infra::Router router;
  ::PhantomLedger::infra::SharedInfra ringInfra;
};

/// Drop accounting emitted by the chronological replay pass.
struct ReplayDrops {
  using ChannelReasonKey = ::PhantomLedger::transfers::legit::ledger::
      ReplayDropLedger::ChannelReasonKey;
  using ChannelReasonHash = ::PhantomLedger::transfers::legit::ledger::
      ReplayDropLedger::ChannelReasonHash;

  std::unordered_map<std::string, std::uint32_t> byReason;
  std::unordered_map<ChannelReasonKey, std::uint32_t, ChannelReasonHash>
      byChannel;
};

/// Ledger state after legitimate and product transfers have been replayed,
/// before fraud is injected.
struct CandidateLedgerReplay {
  std::vector<transactions::Transaction> txns;
  ReplayDrops drops;
};

/// Ledger state after fraud injection and the final replay pass.
struct PostedLedgerReplay {
  std::vector<transactions::Transaction> txns;
  std::unique_ptr<clearing::Ledger> book;
};

/// Transfer ledger artifacts produced by the transfer stage.
struct TransferLedger {
  CandidateLedgerReplay candidate;
  PostedLedgerReplay posted;
};

/// Fraud emission telemetry retained after the injected transaction stream has
/// been folded into the posted ledger.
struct FraudInjectionSummary {
  std::size_t injectedCount = 0;
};

struct Transfers {
  transfers::legit::ledger::LegitTransferResult legit;
  FraudInjectionSummary fraud;
  TransferLedger ledger;
};

} // namespace PhantomLedger::pipeline
