#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

struct SimulateInputs {
  ::PhantomLedger::time::Window window{};
  std::uint64_t seed = 0;

  ::PhantomLedger::pipeline::stages::entities::Inputs entities;
  ::PhantomLedger::pipeline::stages::infra::Inputs infraIn{};

  ::PhantomLedger::pipeline::stages::transfers::RunScope transferRun{};
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
  ::PhantomLedger::pipeline::stages::transfers::PopulationShape
      transferPopulation{};
};

[[nodiscard]] SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                                        const SimulateInputs &in);

} // namespace PhantomLedger::pipeline
