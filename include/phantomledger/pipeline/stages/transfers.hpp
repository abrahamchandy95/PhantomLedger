#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/recurring/policy.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transfers/credit_cards/policy/issuer.hpp"
#include "phantomledger/transfers/family/graph_config.hpp"
#include "phantomledger/transfers/family/transfer_config.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/government/disability.hpp"
#include "phantomledger/transfers/government/retirement.hpp"
#include "phantomledger/transfers/insurance/rates.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::transfers {

struct Inputs {
  /// Time window of the simulation.
  ::PhantomLedger::time::Window window{};

  std::uint64_t seed = 0;

  const ::PhantomLedger::recurring::Policy *recurringPolicy = nullptr;
  const ::PhantomLedger::clearing::BalanceRules *balanceRules = nullptr;
  const ::PhantomLedger::transfers::credit_cards::IssuerPolicy *ccTerms =
      nullptr;
  const ::PhantomLedger::transfers::credit_cards::CardholderBehavior *ccHabits =
      nullptr;
  const ::PhantomLedger::transfers::family::GraphConfig *familyGraphCfg =
      nullptr;
  const ::PhantomLedger::transfers::family::TransferConfig *familyTransferCfg =
      nullptr;

  ::PhantomLedger::transfers::government::RetirementTerms retirement{};
  ::PhantomLedger::transfers::government::DisabilityTerms disability{};

  ::PhantomLedger::transfers::insurance::ClaimRates claimRates{};

  double hubFraction = 0.01;

  ::PhantomLedger::transfers::legit::ledger::ReplayPolicy preReplayPolicy =
      ::PhantomLedger::transfers::legit::ledger::kDefaultReplayPolicy;

  ::PhantomLedger::transfers::fraud::Parameters fraudParams{};
};

[[nodiscard]] ::PhantomLedger::pipeline::Transfers
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      const ::PhantomLedger::pipeline::Infra &infra, const Inputs &in);

} // namespace PhantomLedger::pipeline::stages::transfers
