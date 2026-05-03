#pragma once

#include "phantomledger/entities/synth/people/fraud.hpp"
#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/state.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/recurring/employment.hpp"
#include "phantomledger/recurring/lease.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
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

namespace PhantomLedger::relationships::family {
struct Households;
struct Dependents;
struct RetireeSupport;
} // namespace PhantomLedger::relationships::family

namespace PhantomLedger::transfers::legit::routines::relatives {
struct FamilyTransferModel;
} // namespace PhantomLedger::transfers::legit::routines::relatives

namespace PhantomLedger::pipeline::stages::transfers {

struct RunScope {
  ::PhantomLedger::time::Window window{};
  std::uint64_t seed = 0;
};

struct PopulationShape {
  double hubFraction = 0.01;

  void validate(::PhantomLedger::primitives::validate::Report &r) const {
    namespace v = ::PhantomLedger::primitives::validate;
    r.check([&] { v::unit("hubFraction", hubFraction); });
  }
};

struct RecurringIncome {
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

struct BalanceBookRules {
  const ::PhantomLedger::clearing::BalanceRules *balanceRules = nullptr;
};

struct CreditCardLifecycle {
  const ::PhantomLedger::transfers::credit_cards::LifecycleRules *lifecycle =
      nullptr;
};

struct FamilyPrograms {
  const ::PhantomLedger::relationships::family::Households *households =
      nullptr;
  const ::PhantomLedger::relationships::family::Dependents *dependents =
      nullptr;
  const ::PhantomLedger::relationships::family::RetireeSupport *retireeSupport =
      nullptr;
  const ::PhantomLedger::transfers::legit::routines::relatives::
      FamilyTransferModel *transfers = nullptr;
};

struct GovernmentPrograms {
  ::PhantomLedger::transfers::government::RetirementTerms retirement{};
  ::PhantomLedger::transfers::government::DisabilityTerms disability{};
};

struct InsuranceClaims {
  ::PhantomLedger::transfers::insurance::ClaimRates claimRates{};
};

struct LedgerReplay {
  ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
      preFraud{};
};

struct FraudInjection {
  const ::PhantomLedger::entities::synth::people::Fraud *profile = nullptr;
  ::PhantomLedger::transfers::fraud::Injector::Rules rules;
};

[[nodiscard]] ::PhantomLedger::pipeline::Transfers
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      const ::PhantomLedger::pipeline::Infra &infra, RunScope run = {},
      const RecurringIncome &income = {},
      const BalanceBookRules &balanceBook = {},
      const CreditCardLifecycle &creditCards = {},
      const FamilyPrograms &family = {},
      const GovernmentPrograms &government = {},
      const InsuranceClaims &insurance = {}, const LedgerReplay &replay = {},
      const FraudInjection &fraud = {}, PopulationShape population = {});

} // namespace PhantomLedger::pipeline::stages::transfers
