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
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/ledger/result.hpp"

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

struct Transfers {
  transfers::legit::ledger::TransfersPayload legit;
  transfers::fraud::InjectionOutput fraud;

  std::vector<transactions::Transaction> draftTxns;
  std::vector<transactions::Transaction> finalTxns;

  std::unique_ptr<clearing::Ledger> finalBook;

  std::unordered_map<std::string, std::uint32_t> dropCounts;

  using ChannelReasonKey = ::PhantomLedger::transfers::legit::ledger::
      ChronoReplayAccumulator::ChannelReasonKey;
  using ChannelReasonHash = ::PhantomLedger::transfers::legit::ledger::
      ChronoReplayAccumulator::ChannelReasonHash;

  std::unordered_map<ChannelReasonKey, std::uint32_t, ChannelReasonHash>
      dropCountsByChannel;
};

} // namespace PhantomLedger::pipeline
