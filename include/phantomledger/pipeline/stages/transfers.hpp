#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/recurring/employment.hpp"
#include "phantomledger/recurring/lease.hpp"
#include "phantomledger/transfers/fraud/engine.hpp"
#include "phantomledger/transfers/government/disability.hpp"
#include "phantomledger/transfers/government/retirement.hpp"
#include "phantomledger/transfers/insurance/rates.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"

#include <cstdint>

namespace PhantomLedger::clearing {
struct BalanceRules;
} // namespace PhantomLedger::clearing

namespace PhantomLedger::transfers::credit_cards {
struct LifecycleRules;
} // namespace PhantomLedger::transfers::credit_cards

namespace PhantomLedger::transfers::family {
struct GraphConfig;
} // namespace PhantomLedger::transfers::family

namespace PhantomLedger::transfers::legit::routines::relatives {
struct FamilyTransferModel;
} // namespace PhantomLedger::transfers::legit::routines::relatives

namespace PhantomLedger::pipeline::stages::transfers {

struct IncomeInputs {
  ::PhantomLedger::recurring::EmploymentRules employment{};
  ::PhantomLedger::recurring::LeaseRules lease{};

  double salaryPaidFraction = 0.95;
  double rentPaidFraction = 0.80;

  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;

    employment.validate(r);
    lease.validate(r);

    r.check([&] { v::unit("salaryPaidFraction", salaryPaidFraction); });
    r.check([&] { v::unit("rentPaidFraction", rentPaidFraction); });
  }
};

struct ClearingInputs {
  const ::PhantomLedger::clearing::BalanceRules *balanceRules = nullptr;
};

struct CreditCardInputs {
  const ::PhantomLedger::transfers::credit_cards::LifecycleRules *lifecycle =
      nullptr;
};

struct FamilyInputs {
  const ::PhantomLedger::transfers::family::GraphConfig *graph = nullptr;
  const ::PhantomLedger::transfers::legit::routines::relatives::
      FamilyTransferModel *transfers = nullptr;
};

struct GovernmentInputs {
  ::PhantomLedger::transfers::government::RetirementTerms retirement{};
  ::PhantomLedger::transfers::government::DisabilityTerms disability{};
};

struct InsuranceInputs {
  ::PhantomLedger::transfers::insurance::ClaimRates claimRates{};
};

struct ReplayInputs {
  ::PhantomLedger::transfers::legit::ledger::ReplayPolicy preFraud =
      ::PhantomLedger::transfers::legit::ledger::kDefaultReplayPolicy;
};

struct FraudInputs {
  ::PhantomLedger::transfers::fraud::Parameters params{};
};

struct Inputs {
  ::PhantomLedger::time::Window window{};

  std::uint64_t seed = 0;

  IncomeInputs income{};
  ClearingInputs clearing{};
  CreditCardInputs creditCards{};
  FamilyInputs family{};
  GovernmentInputs government{};
  InsuranceInputs insurance{};
  ReplayInputs replay{};
  FraudInputs fraud{};

  double hubFraction = 0.01;

  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;

    income.validate(r);
    r.check([&] { v::unit("hubFraction", hubFraction); });
  }
};

[[nodiscard]] ::PhantomLedger::pipeline::Transfers
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      const ::PhantomLedger::pipeline::Infra &infra, const Inputs &in);

} // namespace PhantomLedger::pipeline::stages::transfers
