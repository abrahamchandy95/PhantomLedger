#pragma once

#include "phantomledger/activity/income/rent.hpp"
#include "phantomledger/activity/income/salary.hpp"
#include "phantomledger/activity/recurring/employment.hpp"
#include "phantomledger/activity/recurring/lease.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transfers/channels/government/disability.hpp"
#include "phantomledger/transfers/channels/government/retirement.hpp"
#include "phantomledger/transfers/legit/ledger/builder.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include <cstdint>

namespace PhantomLedger::clearing {
struct BalanceRules;
} // namespace PhantomLedger::clearing

namespace PhantomLedger::transfers::credit_cards {
struct LifecycleRules;
} // namespace PhantomLedger::transfers::credit_cards

namespace PhantomLedger::pipeline::stages::transfers {

using FamilyTransferScenario = ::PhantomLedger::transfers::legit::routines::
    relatives::FamilyTransferScenario;

class LegitAssembly {
public:
  struct RunScope {
    ::PhantomLedger::time::Window window{};
    std::uint64_t seed = 0;
  };

  /// Income-side programs. Each subsystem owns its own rules; there is
  /// no single cross-cutting "recurring rules" bag.
  struct IncomePrograms {
    ::PhantomLedger::inflows::salary::Rules salary{};
    ::PhantomLedger::inflows::rent::Rules rent{};
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

  struct HubSelection {
    double fraction = 0.01;
  };

  LegitAssembly() = default;

  LegitAssembly &runScope(RunScope value) noexcept;
  LegitAssembly &incomePrograms(const IncomePrograms &value);
  LegitAssembly &openingBalances(OpeningBalances value) noexcept;
  LegitAssembly &cardLifecycle(CardLifecycle value) noexcept;
  LegitAssembly &familyTransfers(FamilyTransferScenario value) noexcept;
  LegitAssembly &hubSelection(HubSelection value) noexcept;

  LegitAssembly &window(::PhantomLedger::time::Window value) noexcept;
  LegitAssembly &seed(std::uint64_t value) noexcept;

  // Granular rule setters route to the owning subsystem's struct.
  LegitAssembly &
  salaryRules(const ::PhantomLedger::inflows::salary::Rules &value);
  LegitAssembly &rentRules(const ::PhantomLedger::inflows::rent::Rules &value);

  LegitAssembly &
  employmentRules(const ::PhantomLedger::recurring::EmploymentRules &value);
  LegitAssembly &
  leaseRules(const ::PhantomLedger::recurring::LeaseRules &value);
  LegitAssembly &salaryPaidFraction(double value) noexcept;
  LegitAssembly &rentPaidFraction(double value) noexcept;

  LegitAssembly &openingBalanceRules(
      const ::PhantomLedger::clearing::BalanceRules *value) noexcept;
  LegitAssembly &
  creditLifecycle(const ::PhantomLedger::transfers::credit_cards::LifecycleRules
                      *value) noexcept;
  LegitAssembly &family(FamilyTransferScenario value) noexcept;

  LegitAssembly &retirementBenefits(
      const ::PhantomLedger::transfers::government::RetirementTerms &value);
  LegitAssembly &disabilityBenefits(
      const ::PhantomLedger::transfers::government::DisabilityTerms &value);

  [[nodiscard]] const RunScope &runScope() const noexcept { return run_; }
  [[nodiscard]] const IncomePrograms &incomePrograms() const noexcept {
    return income_;
  }
  [[nodiscard]] HubSelection hubSelection() const noexcept {
    return hubSelection_;
  }

  void validate() const;

  [[nodiscard]] ::PhantomLedger::transfers::legit::ledger::LegitTransferBuilder
  builder(::PhantomLedger::random::Rng &rng,
          const ::PhantomLedger::pipeline::Entities &entities,
          const ::PhantomLedger::pipeline::Infra &infra) const;

private:
  RunScope run_{};
  IncomePrograms income_{};
  OpeningBalances openingBalances_{};
  CardLifecycle cardLifecycle_{};
  FamilyTransferScenario familyTransfers_{};
  HubSelection hubSelection_{};
};

} // namespace PhantomLedger::pipeline::stages::transfers
