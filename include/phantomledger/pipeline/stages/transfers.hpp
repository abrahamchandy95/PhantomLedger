#pragma once

#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/government/disability.hpp"
#include "phantomledger/transfers/government/retirement.hpp"
#include "phantomledger/transfers/insurance/rates.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace PhantomLedger::clearing {
struct BalanceRules;
} // namespace PhantomLedger::clearing

namespace PhantomLedger::transfers::credit_cards {
struct LifecycleRules;
} // namespace PhantomLedger::transfers::credit_cards

namespace PhantomLedger::transfers::legit::ledger {
class LegitTransferBuilder;
struct TransfersPayload;
} // namespace PhantomLedger::transfers::legit::ledger

namespace PhantomLedger::pipeline::stages::transfers {

using FamilyTransferScenario = ::PhantomLedger::transfers::legit::routines::
    relatives::FamilyTransferScenario;

class TransferStage {
public:
  struct RunScope {
    ::PhantomLedger::time::Window window{};
    std::uint64_t seed = 0;
  };

  struct IncomePrograms {
    ::PhantomLedger::inflows::RecurringIncomeRules recurring{};
    ::PhantomLedger::transfers::government::RetirementTerms retirement{};
    ::PhantomLedger::transfers::government::DisabilityTerms disability{};
  };

  struct OpeningBalances {
    const ::PhantomLedger::clearing::BalanceRules *balanceRules = nullptr;
  };

  struct CardLifecycle {
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules
        *lifecycleRules = nullptr;
  };

  struct InsurancePrograms {
    ::PhantomLedger::transfers::insurance::ClaimRates claimRates{};
  };

  struct ReplayOrdering {
    ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
        replayRules{};
  };

  struct FraudInjection {
    const ::PhantomLedger::entities::synth::people::Fraud *profile = nullptr;
    ::PhantomLedger::transfers::fraud::Injector::Rules injectorRules{};
  };

  struct HubSelection {
    double fraction = 0.01;
  };

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
  using Transaction = ::PhantomLedger::transactions::Transaction;
  using PrimaryAccounts = std::unordered_map<::PhantomLedger::entity::PersonId,
                                             ::PhantomLedger::entity::Key>;

  ::PhantomLedger::random::Rng *rng_ = nullptr;
  const ::PhantomLedger::pipeline::Entities *entities_ = nullptr;
  const ::PhantomLedger::pipeline::Infra *infra_ = nullptr;

  RunScope run_{};
  IncomePrograms income_{};
  OpeningBalances openingBalances_{};
  CardLifecycle cardLifecycle_{};
  FamilyTransferScenario familyTransfers_{};
  InsurancePrograms insurance_{};
  ReplayOrdering replay_{};
  FraudInjection fraud_{};
  HubSelection hubSelection_{};

  [[nodiscard]] PrimaryAccounts primaryAccounts() const;
  [[nodiscard]] ::PhantomLedger::transfers::legit::ledger::LegitTransferBuilder
  makeLegitBuilder() const;
  [[nodiscard]] std::vector<Transaction>
  replayStream(const PrimaryAccounts &primaryAccounts,
               ::PhantomLedger::transfers::legit::ledger::TransfersPayload
                   &legitPayload) const;
  [[nodiscard]] ::PhantomLedger::transfers::fraud::InjectionOutput
  injectFraud(std::span<const Transaction> draftTxns,
              const ::PhantomLedger::transfers::legit::ledger::TransfersPayload
                  &legitPayload) const;
};

} // namespace PhantomLedger::pipeline::stages::transfers
