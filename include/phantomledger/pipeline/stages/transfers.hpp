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
  TransferStage(::PhantomLedger::random::Rng &rng,
                const ::PhantomLedger::pipeline::Entities &entities,
                const ::PhantomLedger::pipeline::Infra &infra) noexcept;

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

  ::PhantomLedger::time::Window window_{};
  std::uint64_t seed_ = 0;

  ::PhantomLedger::inflows::RecurringIncomeRules recurringIncome_{};
  const ::PhantomLedger::clearing::BalanceRules *openingBalanceRules_ = nullptr;
  const ::PhantomLedger::transfers::credit_cards::LifecycleRules
      *creditLifecycle_ = nullptr;
  FamilyTransferScenario familyScenario_{};
  ::PhantomLedger::transfers::government::RetirementTerms retirement_{};
  ::PhantomLedger::transfers::government::DisabilityTerms disability_{};
  ::PhantomLedger::transfers::insurance::ClaimRates claimRates_{};
  ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
      replayRules_{};
  const ::PhantomLedger::entities::synth::people::Fraud *fraudProfile_ =
      nullptr;
  ::PhantomLedger::transfers::fraud::Injector::Rules fraudRules_{};
  double hubFraction_ = 0.01;

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
