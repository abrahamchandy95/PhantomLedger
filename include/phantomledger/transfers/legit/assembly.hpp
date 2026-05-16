#pragma once

#include "phantomledger/activity/income/rent.hpp"
#include "phantomledger/activity/income/salary.hpp"
#include "phantomledger/activity/recurring/employment.hpp"
#include "phantomledger/activity/recurring/lease.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/transfers/channels/government/disability.hpp"
#include "phantomledger/transfers/channels/government/retirement.hpp"
#include "phantomledger/transfers/legit/ledger/builder.hpp"
#include "phantomledger/transfers/legit/routines/relatives.hpp"

#include <cstdint>

namespace PhantomLedger::clearing {
struct BalanceRules;
}

namespace PhantomLedger::transfers::credit_cards {
struct LifecycleRules;
}

namespace PhantomLedger::transfers::legit {

using FamilyTransferScenario = routines::relatives::FamilyTransferScenario;

class LegitAssembly {
public:
  struct RunScope {
    time::Window window{};
    std::uint64_t seed = 0;
  };

  struct IncomePrograms {
    activity::income::salary::Rules salary{};
    activity::income::rent::Rules rent{};
    government::RetirementTerms retirement{};
    government::DisabilityTerms disability{};
  };

  struct OpeningBalances {
    const clearing::BalanceRules *balanceRules = nullptr;
  };

  struct CardLifecycle {
    const credit_cards::LifecycleRules *lifecycleRules = nullptr;
  };

  struct HubSelection {
    double fraction = 0.01;
  };

  LegitAssembly();

  LegitAssembly &runScope(RunScope value) noexcept;
  LegitAssembly &incomePrograms(const IncomePrograms &value);
  LegitAssembly &openingBalances(OpeningBalances value) noexcept;
  LegitAssembly &cardLifecycle(CardLifecycle value) noexcept;
  LegitAssembly &familyTransfers(FamilyTransferScenario value) noexcept;
  LegitAssembly &hubSelection(HubSelection value) noexcept;

  LegitAssembly &window(time::Window value) noexcept;
  LegitAssembly &seed(std::uint64_t value) noexcept;

  LegitAssembly &salaryRules(const activity::income::salary::Rules &value);
  LegitAssembly &rentRules(const activity::income::rent::Rules &value);

  LegitAssembly &
  employmentRules(const activity::recurring::EmploymentRules &value);
  LegitAssembly &leaseRules(const activity::recurring::LeaseRules &value);
  LegitAssembly &salaryPaidFraction(double value) noexcept;
  LegitAssembly &rentPaidFraction(double value) noexcept;

  LegitAssembly &
  openingBalanceRules(const clearing::BalanceRules *value) noexcept;
  LegitAssembly &
  creditLifecycle(const credit_cards::LifecycleRules *value) noexcept;
  LegitAssembly &family(FamilyTransferScenario value) noexcept;

  LegitAssembly &retirementBenefits(const government::RetirementTerms &value);
  LegitAssembly &disabilityBenefits(const government::DisabilityTerms &value);

  [[nodiscard]] const RunScope &runScope() const noexcept { return run_; }
  [[nodiscard]] const IncomePrograms &incomePrograms() const noexcept {
    return income_;
  }
  [[nodiscard]] HubSelection hubSelection() const noexcept {
    return hubSelection_;
  }

  void validate() const;

  [[nodiscard]] ledger::LegitTransferBuilder
  builder(random::Rng &rng, const pipeline::People &people,
          const pipeline::Holdings &holdings,
          const pipeline::Counterparties &cps) const;

private:
  RunScope run_{};
  IncomePrograms income_{};
  OpeningBalances openingBalances_{};
  CardLifecycle cardLifecycle_{};
  FamilyTransferScenario familyTransfers_{};
  HubSelection hubSelection_{};
};

} // namespace PhantomLedger::transfers::legit
