#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

class SimulationPipeline {
public:
  SimulationPipeline(
      ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
      std::uint64_t seed = 0);

  SimulationPipeline(const SimulationPipeline &) = delete;
  SimulationPipeline &operator=(const SimulationPipeline &) = delete;
  SimulationPipeline(SimulationPipeline &&) noexcept = delete;
  SimulationPipeline &operator=(SimulationPipeline &&) noexcept = delete;

  SimulationPipeline &infraWindow(::PhantomLedger::time::Window value) noexcept;
  SimulationPipeline &ringAccess(
      const ::PhantomLedger::infra::synth::rings::AccessRules &value) noexcept;
  SimulationPipeline &
  deviceAssignment(const ::PhantomLedger::infra::synth::devices::AssignmentRules
                       &value) noexcept;
  SimulationPipeline &
  ipAssignment(const ::PhantomLedger::infra::synth::ips::AssignmentRules
                   &value) noexcept;
  SimulationPipeline &routingBehavior(
      ::PhantomLedger::pipeline::stages::infra::RoutingBehavior value) noexcept;
  SimulationPipeline &sharedInfra(
      ::PhantomLedger::pipeline::stages::infra::SharedInfraUse value) noexcept;

  SimulationPipeline &
  transferWindow(::PhantomLedger::time::Window value) noexcept;
  SimulationPipeline &transferSeed(std::uint64_t value) noexcept;

  SimulationPipeline &
  recurringIncome(const ::PhantomLedger::inflows::RecurringIncomeRules &value);
  SimulationPipeline &
  employmentRules(const ::PhantomLedger::recurring::EmploymentRules &value);
  SimulationPipeline &
  leaseRules(const ::PhantomLedger::recurring::LeaseRules &value);
  SimulationPipeline &salaryPaidFraction(double value) noexcept;
  SimulationPipeline &rentPaidFraction(double value) noexcept;

  SimulationPipeline &openingBalanceRules(
      const ::PhantomLedger::clearing::BalanceRules *value) noexcept;
  SimulationPipeline &
  creditLifecycle(const ::PhantomLedger::transfers::credit_cards::LifecycleRules
                      *value) noexcept;
  SimulationPipeline &
  family(::PhantomLedger::pipeline::stages::transfers::FamilyTransferScenario
             value) noexcept;

  SimulationPipeline &retirementBenefits(
      const ::PhantomLedger::transfers::government::RetirementTerms &value);
  SimulationPipeline &disabilityBenefits(
      const ::PhantomLedger::transfers::government::DisabilityTerms &value);
  SimulationPipeline &insuranceClaims(
      ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept;

  SimulationPipeline &replayRules(
      ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
          value) noexcept;
  SimulationPipeline &fraudProfile(
      const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept;
  SimulationPipeline &
  fraudRules(::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept;
  SimulationPipeline &hubFraction(double value) noexcept;

  [[nodiscard]] SimulationResult run() const;

private:
  ::PhantomLedger::random::Rng *rng_ = nullptr;
  ::PhantomLedger::time::Window window_{};
  std::uint64_t seed_ = 0;

  ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities_;

  ::PhantomLedger::pipeline::stages::infra::AccessInfraStage infra_{};

  ::PhantomLedger::time::Window transferWindow_{};
  std::uint64_t transferSeed_ = 0;
  ::PhantomLedger::inflows::RecurringIncomeRules recurringIncome_{};
  const ::PhantomLedger::clearing::BalanceRules *openingBalanceRules_ = nullptr;
  const ::PhantomLedger::transfers::credit_cards::LifecycleRules
      *creditLifecycle_ = nullptr;
  ::PhantomLedger::pipeline::stages::transfers::FamilyTransferScenario
      familyScenario_{};
  ::PhantomLedger::transfers::government::RetirementTerms retirement_{};
  ::PhantomLedger::transfers::government::DisabilityTerms disability_{};
  ::PhantomLedger::transfers::insurance::ClaimRates claimRates_{};
  ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
      replayRules_{};
  const ::PhantomLedger::entities::synth::people::Fraud *fraudProfile_ =
      nullptr;
  ::PhantomLedger::transfers::fraud::Injector::Rules fraudRules_{};
  double hubFraction_ = 0.01;
};

[[nodiscard]] SimulationResult
simulate(::PhantomLedger::random::Rng &rng,
         ::PhantomLedger::time::Window window,
         ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
         std::uint64_t seed = 0);

} // namespace PhantomLedger::pipeline
