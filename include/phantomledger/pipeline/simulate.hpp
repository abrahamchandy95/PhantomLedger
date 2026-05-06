#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/products.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

class SimulationPipeline {
public:
  using ObligationProducts =
      ::PhantomLedger::pipeline::stages::products::ObligationPrograms;
  using TransferStage =
      ::PhantomLedger::pipeline::stages::transfers::TransferStage;

  SimulationPipeline(
      ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
      std::uint64_t seed = 0);

  SimulationPipeline(
      ::PhantomLedger::random::Rng &rng, ::PhantomLedger::time::Window window,
      ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
      ObligationProducts obligationProducts, std::uint64_t seed = 0);

  SimulationPipeline(const SimulationPipeline &) = delete;
  SimulationPipeline &operator=(const SimulationPipeline &) = delete;
  SimulationPipeline(SimulationPipeline &&) noexcept = delete;
  SimulationPipeline &operator=(SimulationPipeline &&) noexcept = delete;

  [[nodiscard]] TransferStage &transferStage() noexcept;
  [[nodiscard]] const TransferStage &transferStage() const noexcept;

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

  SimulationPipeline &obligationProducts(ObligationProducts value) noexcept;
  SimulationPipeline &
  productRng(::PhantomLedger::pipeline::stages::products::PersonProductRng
                 value) noexcept;
  SimulationPipeline &installmentLending(
      ::PhantomLedger::pipeline::stages::products::InstallmentLending
          value) noexcept;
  SimulationPipeline &
  taxWithholding(::PhantomLedger::pipeline::stages::products::TaxWithholding
                     value) noexcept;
  SimulationPipeline &insuranceCoverage(
      ::PhantomLedger::pipeline::stages::products::InsuranceCoverage
          value) noexcept;

  SimulationPipeline &transferScope(TransferStage::RunScope value) noexcept;
  SimulationPipeline &
  transferIncome(const TransferStage::IncomePrograms &value);
  SimulationPipeline &
  openingBalances(TransferStage::OpeningBalances value) noexcept;
  SimulationPipeline &
  cardLifecycle(TransferStage::CardLifecycle value) noexcept;
  SimulationPipeline &familyTransfers(
      ::PhantomLedger::pipeline::stages::transfers::FamilyTransferScenario
          value) noexcept;
  SimulationPipeline &
  insurancePrograms(TransferStage::InsurancePrograms value) noexcept;
  SimulationPipeline &
  replayOrdering(TransferStage::ReplayOrdering value) noexcept;
  SimulationPipeline &
  fraudInjection(TransferStage::FraudInjection value) noexcept;
  SimulationPipeline &hubSelection(TransferStage::HubSelection value) noexcept;

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
  ObligationProducts obligationProducts_{};

  ::PhantomLedger::pipeline::stages::infra::AccessInfraStage infra_{};
  TransferStage transfers_{};
};

[[nodiscard]] SimulationResult
simulate(::PhantomLedger::random::Rng &rng,
         ::PhantomLedger::time::Window window,
         ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
         std::uint64_t seed = 0);

[[nodiscard]] SimulationResult
simulate(::PhantomLedger::random::Rng &rng,
         ::PhantomLedger::time::Window window,
         ::PhantomLedger::pipeline::stages::entities::EntitySynthesis entities,
         ::PhantomLedger::pipeline::stages::products::ObligationPrograms
             obligationProducts,
         std::uint64_t seed = 0);

} // namespace PhantomLedger::pipeline
