#pragma once

#include "phantomledger/activity/income/rent.hpp"
#include "phantomledger/activity/income/salary.hpp"
#include "phantomledger/activity/recurring/employment.hpp"
#include "phantomledger/activity/recurring/lease.hpp"
#include "phantomledger/entities/cards.hpp"
#include "phantomledger/entities/counterparties.hpp"
#include "phantomledger/entities/merchants.hpp"
#include "phantomledger/entities/products/portfolio.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/accounts/pack.hpp"
#include "phantomledger/synth/landlords/pack.hpp"
#include "phantomledger/synth/personas/pack.hpp"
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

  // The synthesized world exactly as the legit assembly consumes it
  // (round C-2 dependency inversion: the consumer names what it needs;
  // the pipeline adapts its bundles at legitWorldInputs()). Every
  // pointer is non-owning and must outlive the returned builder.
  struct WorldInputs {
    // Census
    const synth::personas::Pack *personas = nullptr;
    std::uint32_t populationCount = 0;

    // Holdings
    const synth::accounts::Pack *accounts = nullptr;
    const entity::card::Registry *creditCards = nullptr;
    const entity::product::PortfolioRegistry *portfolios = nullptr;

    // Counterparty universe
    const entity::counterparty::Directory *counterparties = nullptr;
    const synth::landlords::Pack *landlords = nullptr;
    const entity::merchant::Catalog *merchants = nullptr;
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
  builder(random::Rng &rng, const WorldInputs &world) const;

private:
  RunScope run_{};
  IncomePrograms income_{};
  OpeningBalances openingBalances_{};
  CardLifecycle cardLifecycle_{};
  FamilyTransferScenario familyTransfers_{};
  HubSelection hubSelection_{};
};

} // namespace PhantomLedger::transfers::legit
