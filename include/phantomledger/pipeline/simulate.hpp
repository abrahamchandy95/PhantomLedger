#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

struct TransferSynthesis {
  ::PhantomLedger::pipeline::stages::transfers::RunScope run{};
  ::PhantomLedger::pipeline::stages::transfers::RecurringIncome
      recurringIncome{};
  ::PhantomLedger::pipeline::stages::transfers::BalanceBookRules balanceBook{};
  ::PhantomLedger::pipeline::stages::transfers::CreditCardLifecycle
      creditCards{};
  ::PhantomLedger::pipeline::stages::transfers::FamilyPrograms family{};
  ::PhantomLedger::pipeline::stages::transfers::GovernmentPrograms government{};
  ::PhantomLedger::pipeline::stages::transfers::InsuranceClaims insurance{};
  ::PhantomLedger::pipeline::stages::transfers::LedgerReplay replay{};
  ::PhantomLedger::pipeline::stages::transfers::FraudInjection fraud{};
  ::PhantomLedger::pipeline::stages::transfers::PopulationShape population{};
};

struct SimulationScenario {
  ::PhantomLedger::time::Window window{};
  std::uint64_t seed = 0;

  ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities;
  ::PhantomLedger::pipeline::stages::infra::InfraSynthesis infra{};
  TransferSynthesis transfers{};
};

[[nodiscard]] SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                                        const SimulationScenario &scenario);

} // namespace PhantomLedger::pipeline
