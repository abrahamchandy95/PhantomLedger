#pragma once

#include "phantomledger/pipeline/stages/transfers/fraud_emission.hpp"
#include "phantomledger/pipeline/stages/transfers/ledger_replay.hpp"
#include "phantomledger/pipeline/stages/transfers/legit_assembly.hpp"
#include "phantomledger/pipeline/stages/transfers/product_replay.hpp"
#include "phantomledger/pipeline/state.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline::stages::transfers {

class TransferStage {
public:
  using RunScope = LegitAssembly::RunScope;
  using IncomePrograms = LegitAssembly::IncomePrograms;
  using OpeningBalances = LegitAssembly::OpeningBalances;
  using CardLifecycle = LegitAssembly::CardLifecycle;
  using HubSelection = LegitAssembly::HubSelection;
  using InsurancePrograms = ProductReplay::InsurancePrograms;
  using ReplayOrdering = LedgerReplay::Ordering;
  using FraudInjection = FraudEmission::Programs;

  TransferStage(::PhantomLedger::random::Rng &rng,
                const ::PhantomLedger::pipeline::Entities &entities,
                const ::PhantomLedger::pipeline::Infra &infra) noexcept;

  TransferStage &runScope(RunScope value) noexcept;
  TransferStage &incomePrograms(const IncomePrograms &value);
  TransferStage &openingBalances(OpeningBalances value) noexcept;
  TransferStage &cardLifecycle(CardLifecycle value) noexcept;
  TransferStage &familyTransfers(FamilyTransferScenario value) noexcept;
  TransferStage &insurancePrograms(InsurancePrograms value) noexcept;
  TransferStage &replayOrdering(ReplayOrdering value) noexcept;
  TransferStage &fraudInjection(FraudInjection value) noexcept;
  TransferStage &hubSelection(HubSelection value) noexcept;

  TransferStage &window(::PhantomLedger::time::Window value) noexcept;
  TransferStage &seed(std::uint64_t value) noexcept;

  TransferStage &
  recurringIncome(const ::PhantomLedger::inflows::RecurringIncomeRules &value);
  TransferStage &
  employmentRules(const ::PhantomLedger::recurring::EmploymentRules &value);
  TransferStage &
  leaseRules(const ::PhantomLedger::recurring::LeaseRules &value);
  TransferStage &salaryPaidFraction(double value) noexcept;
  TransferStage &rentPaidFraction(double value) noexcept;

  TransferStage &openingBalanceRules(
      const ::PhantomLedger::clearing::BalanceRules *value) noexcept;
  TransferStage &
  creditLifecycle(const ::PhantomLedger::transfers::credit_cards::LifecycleRules
                      *value) noexcept;
  TransferStage &family(FamilyTransferScenario value) noexcept;

  TransferStage &retirementBenefits(
      const ::PhantomLedger::transfers::government::RetirementTerms &value);
  TransferStage &disabilityBenefits(
      const ::PhantomLedger::transfers::government::DisabilityTerms &value);
  TransferStage &insuranceClaims(
      ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept;

  TransferStage &replayRules(
      ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
          value) noexcept;
  TransferStage &fraudProfile(
      const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept;
  TransferStage &
  fraudRules(::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept;
  TransferStage &hubFraction(double value) noexcept;

  [[nodiscard]] ::PhantomLedger::pipeline::Transfers build() const;

private:
  [[nodiscard]] const RunScope &runScope() const noexcept;

  ::PhantomLedger::random::Rng *rng_ = nullptr;
  const ::PhantomLedger::pipeline::Entities *entities_ = nullptr;
  const ::PhantomLedger::pipeline::Infra *infra_ = nullptr;

  LegitAssembly legit_{};
  ProductReplay products_{};
  LedgerReplay ledger_{};
  FraudEmission fraud_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
