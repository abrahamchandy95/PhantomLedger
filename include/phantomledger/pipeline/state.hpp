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
#include "phantomledger/transfers/legit/blueprints/plans.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
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
  entity::counterparty::Pool counterpartyPools;
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
  transfers::legit::blueprints::TransfersPayload legit;
  transfers::fraud::InjectionOutput fraud;

  std::vector<transactions::Transaction> draftTxns;
  std::vector<transactions::Transaction> finalTxns;

  std::unique_ptr<clearing::Ledger> finalBook;

  std::unordered_map<std::string, std::uint64_t> dropCounts;

  struct ChannelReason {
    std::string channel;
    std::string reason;

    bool operator==(const ChannelReason &other) const noexcept {
      return channel == other.channel && reason == other.reason;
    }
  };

  struct ChannelReasonHash {
    std::size_t operator()(const ChannelReason &k) const noexcept {
      const std::size_t h1 = std::hash<std::string>{}(k.channel);
      const std::size_t h2 = std::hash<std::string>{}(k.reason);
      return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
  };

  std::unordered_map<ChannelReason, std::uint64_t, ChannelReasonHash>
      dropCountsByChannel;
};

} // namespace PhantomLedger::pipeline
