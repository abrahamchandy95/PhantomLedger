#pragma once

#include "phantomledger/entities/synth/products/auto_loan.hpp"
#include "phantomledger/entities/synth/products/insurance.hpp"
#include "phantomledger/entities/synth/products/mortgage.hpp"
#include "phantomledger/entities/synth/products/student_loan.hpp"
#include "phantomledger/entities/synth/products/tax.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

struct SimulateEntities {
  stages::entities::IdentitySource identity;
  stages::entities::PopulationPlan population{};
  stages::entities::Seeds seeds{};

  ::PhantomLedger::entities::synth::people::Fraud fraud{};
  ::PhantomLedger::entities::synth::personas::Mix personaMix{};
  ::PhantomLedger::entities::synth::merchants::Config merchants{};
  ::PhantomLedger::entities::synth::landlords::Config landlords{};
  ::PhantomLedger::entities::synth::counterparties::Config counterparties{};
  ::PhantomLedger::entities::synth::cards::IssuanceRules cardIssuance{};

  ::PhantomLedger::entities::synth::products::MortgageTerms mortgage{};
  ::PhantomLedger::entities::synth::products::AutoLoanTerms autoLoan{};
  ::PhantomLedger::entities::synth::products::StudentLoanTerms studentLoan{};
  ::PhantomLedger::entities::synth::products::TaxTerms tax{};
  ::PhantomLedger::entities::synth::products::InsuranceTerms insurance{};
};

struct SimulateInputs {
  ::PhantomLedger::time::Window window{};
  std::uint64_t seed = 0;

  SimulateEntities entities;
  ::PhantomLedger::pipeline::stages::infra::Inputs infraIn{};
  ::PhantomLedger::pipeline::stages::transfers::Inputs transfersIn{};
};

[[nodiscard]] SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                                        const SimulateInputs &in);

} // namespace PhantomLedger::pipeline
