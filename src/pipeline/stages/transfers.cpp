#include "phantomledger/pipeline/stages/transfers.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/insurance/claims.hpp"
#include "phantomledger/transfers/insurance/premiums.hpp"
#include "phantomledger/transfers/legit/ledger/builder.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/obligations/schedule.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace PhantomLedger::pipeline::stages::transfers {

namespace {

namespace blueprints = ::PhantomLedger::transfers::legit::blueprints;
namespace legit_ledger = ::PhantomLedger::transfers::legit::ledger;
namespace fraud = ::PhantomLedger::transfers::fraud;
namespace insurance = ::PhantomLedger::transfers::insurance;
namespace obligations = ::PhantomLedger::transfers::obligations;
namespace validate = ::PhantomLedger::primitives::validate;

using Transaction = ::PhantomLedger::transactions::Transaction;
using ChannelReasonKey = ::PhantomLedger::pipeline::Transfers::ChannelReasonKey;
using ChannelReasonHash =
    ::PhantomLedger::pipeline::Transfers::ChannelReasonHash;

constexpr double kFraudHeadroomFraction = 0.05;

[[nodiscard]] std::size_t fraudMergedCapacity(std::size_t legitSize) noexcept {
  return legitSize + static_cast<std::size_t>(static_cast<double>(legitSize) *
                                              kFraudHeadroomFraction);
}

void validateHubFraction(double value) {
  validate::Report report;
  report.check([&] { validate::unit("hubFraction", value); });
  report.throwIfFailed();
}

[[nodiscard]] blueprints::LegitTimeframe
makeLegitTimeframe(::PhantomLedger::time::Window window,
                   std::uint64_t seed) noexcept {
  return blueprints::LegitTimeframe{
      .window = window,
      .seed = seed,
  };
}

[[nodiscard]] blueprints::AccountCensus makeAccountCensus(
    const ::PhantomLedger::pipeline::Entities &entities) noexcept {
  return blueprints::AccountCensus{
      .accounts = &entities.accounts.registry,
      .ownership = &entities.accounts.ownership,
  };
}

[[nodiscard]] blueprints::CounterpartyPools makeCounterpartyPools(
    const ::PhantomLedger::pipeline::Entities &entities) noexcept {
  return blueprints::CounterpartyPools{
      .directory = &entities.counterparties,
      .landlords = &entities.landlords.roster,
  };
}

[[nodiscard]] blueprints::PersonaCatalog makePersonaCatalog(
    const ::PhantomLedger::pipeline::Entities &entities) noexcept {
  return blueprints::PersonaCatalog{
      .pack = &entities.personas,
  };
}

[[nodiscard]] blueprints::HubSelectionRules
makeHubSelection(const ::PhantomLedger::pipeline::Entities &entities,
                 double hubFraction) noexcept {
  return blueprints::HubSelectionRules{
      .populationCount = entities.people.roster.count,
      .fraction = hubFraction,
  };
}

[[nodiscard]] legit_ledger::OpeningBook makeOpeningBook(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::clearing::BalanceRules *balanceRules) noexcept {
  return legit_ledger::OpeningBook{
      rng,
      legit_ledger::OpeningBook::Accounts{
          .registry = &entities.accounts.registry,
          .lookup = &entities.accounts.lookup,
          .ownership = &entities.accounts.ownership,
      },
      legit_ledger::OpeningBook::Protections{
          .balanceRules = balanceRules,
          .portfolios = &entities.portfolios,
          .creditCards = &entities.creditCards,
      },
  };
}

[[nodiscard]] legit_ledger::passes::IncomePass makeIncomePass(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::inflows::RecurringIncomeRules &recurringIncome,
    const ::PhantomLedger::transfers::government::RetirementTerms &retirement,
    const ::PhantomLedger::transfers::government::DisabilityTerms &disability) {
  return legit_ledger::passes::IncomePass{
      &rng,
      legit_ledger::passes::AccountAccess{
          .registry = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
      },
      &entities.counterparties,
      recurringIncome,
      legit_ledger::passes::GovernmentBenefits{
          .retirement = &retirement,
          .disability = &disability,
      },
  };
}

[[nodiscard]] legit_ledger::passes::RoutinePass makeRoutinePass(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::inflows::RecurringIncomeRules &recurringIncome) {
  return legit_ledger::passes::RoutinePass{
      &rng,
      legit_ledger::passes::AccountAccess{
          .registry = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
      },
      legit_ledger::passes::RoutineResources{
          .accountsLookup = &entities.accounts.lookup,
          .merchants = &entities.merchants.catalog,
          .portfolios = &entities.portfolios,
          .creditCards = &entities.creditCards,
      },
      recurringIncome,
  };
}

[[nodiscard]] legit_ledger::passes::FamilyPass
makeFamilyPass(const ::PhantomLedger::pipeline::Entities &entities) {
  return legit_ledger::passes::FamilyPass{
      legit_ledger::passes::AccountAccess{
          .registry = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
      },
      &entities.merchants.catalog,
  };
}

[[nodiscard]] legit_ledger::passes::CreditLifecyclePass
makeCreditPass(::PhantomLedger::random::Rng &rng,
               const ::PhantomLedger::pipeline::Entities &entities,
               const ::PhantomLedger::transfers::credit_cards::LifecycleRules
                   *lifecycleRules) {
  return legit_ledger::passes::CreditLifecyclePass{
      &rng,
      &entities.creditCards,
      lifecycleRules,
  };
}

struct PreReplayResult {
  std::vector<Transaction> draftTxns;
  std::unordered_map<std::string, std::uint32_t> dropCounts;
  std::unordered_map<ChannelReasonKey, std::uint32_t, ChannelReasonHash>
      dropCountsByChannel;
};

[[nodiscard]] PreReplayResult
runPreFraudReplay(const ::PhantomLedger::clearing::Ledger &initialBook,
                  ::PhantomLedger::random::Rng &rng,
                  legit_ledger::ChronoReplayAccumulator::Rules rules,
                  std::vector<Transaction> sorted) {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, rules, /*emitLiquidityEvents=*/true);

  accumulator.extend(std::move(sorted), /*presorted=*/true);

  PreReplayResult out;
  out.draftTxns = accumulator.takeTxns();
  out.dropCounts = accumulator.dropCounts();
  out.dropCountsByChannel = accumulator.dropCountsByChannel();

  return out;
}

struct PostReplayResult {
  std::vector<Transaction> finalTxns;
  std::unique_ptr<::PhantomLedger::clearing::Ledger> finalBook;
};

[[nodiscard]] PostReplayResult
runPostFraudReplay(::PhantomLedger::random::Rng &rng,
                   const ::PhantomLedger::clearing::Ledger &initialBook,
                   legit_ledger::ChronoReplayAccumulator::Rules rules,
                   std::vector<Transaction> merged) {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, rules, /*emitLiquidityEvents=*/false);

  auto sorted = legit_ledger::sortForReplay(
      std::span<const Transaction>{merged.data(), merged.size()});
  accumulator.extend(std::move(sorted), /*presorted=*/true);

  PostReplayResult out;
  out.finalTxns = accumulator.takeTxns();
  out.finalBook = std::move(bookCopy);

  return out;
}

} // namespace

TransferStage::TransferStage(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::pipeline::Infra &infra) noexcept
    : rng_{&rng}, entities_{&entities}, infra_{&infra} {}

TransferStage &
TransferStage::window(::PhantomLedger::time::Window value) noexcept {
  window_ = value;
  return *this;
}

TransferStage &TransferStage::seed(std::uint64_t value) noexcept {
  seed_ = value;
  return *this;
}

TransferStage &TransferStage::recurringIncome(
    const ::PhantomLedger::inflows::RecurringIncomeRules &value) {
  recurringIncome_ = value;
  return *this;
}

TransferStage &TransferStage::employmentRules(
    const ::PhantomLedger::recurring::EmploymentRules &value) {
  recurringIncome_.employment = value;
  return *this;
}

TransferStage &
TransferStage::leaseRules(const ::PhantomLedger::recurring::LeaseRules &value) {
  recurringIncome_.lease = value;
  return *this;
}

TransferStage &TransferStage::salaryPaidFraction(double value) noexcept {
  recurringIncome_.salaryPaidFraction = value;
  return *this;
}

TransferStage &TransferStage::rentPaidFraction(double value) noexcept {
  recurringIncome_.rentPaidFraction = value;
  return *this;
}

TransferStage &TransferStage::openingBalanceRules(
    const ::PhantomLedger::clearing::BalanceRules *value) noexcept {
  openingBalanceRules_ = value;
  return *this;
}

TransferStage &TransferStage::creditLifecycle(
    const ::PhantomLedger::transfers::credit_cards::LifecycleRules
        *value) noexcept {
  creditLifecycle_ = value;
  return *this;
}

TransferStage &TransferStage::family(FamilyTransferScenario value) noexcept {
  familyScenario_ = value;
  return *this;
}

TransferStage &TransferStage::retirementBenefits(
    const ::PhantomLedger::transfers::government::RetirementTerms &value) {
  retirement_ = value;
  return *this;
}

TransferStage &TransferStage::disabilityBenefits(
    const ::PhantomLedger::transfers::government::DisabilityTerms &value) {
  disability_ = value;
  return *this;
}

TransferStage &TransferStage::insuranceClaims(
    ::PhantomLedger::transfers::insurance::ClaimRates value) noexcept {
  claimRates_ = value;
  return *this;
}

TransferStage &TransferStage::replayRules(
    ::PhantomLedger::transfers::legit::ledger::ChronoReplayAccumulator::Rules
        value) noexcept {
  replayRules_ = value;
  return *this;
}

TransferStage &TransferStage::fraudProfile(
    const ::PhantomLedger::entities::synth::people::Fraud *value) noexcept {
  fraudProfile_ = value;
  return *this;
}

TransferStage &TransferStage::fraudRules(
    ::PhantomLedger::transfers::fraud::Injector::Rules value) noexcept {
  fraudRules_ = value;
  return *this;
}

TransferStage &TransferStage::hubFraction(double value) noexcept {
  hubFraction_ = value;
  return *this;
}

TransferStage::PrimaryAccounts TransferStage::primaryAccounts() const {
  PrimaryAccounts out;
  out.reserve(entities_->accounts.registry.records.size());

  for (const auto &record : entities_->accounts.registry.records) {
    if (record.owner == ::PhantomLedger::entity::invalidPerson) {
      continue;
    }

    out.try_emplace(record.owner, record.id);
  }

  return out;
}

legit_ledger::LegitTransferBuilder TransferStage::makeLegitBuilder() const {
  legit_ledger::LegitTransferBuilder builder{
      *rng_,
      makeLegitTimeframe(window_, seed_),
      makeAccountCensus(*entities_),
      makeOpeningBook(*rng_, *entities_, openingBalanceRules_),
  };

  builder.counterparties(makeCounterpartyPools(*entities_))
      .personas(makePersonaCatalog(*entities_))
      .hubSelection(makeHubSelection(*entities_, hubFraction_))
      .income(makeIncomePass(*rng_, *entities_, recurringIncome_, retirement_,
                             disability_))
      .routines(makeRoutinePass(*rng_, *entities_, recurringIncome_))
      .family(makeFamilyPass(*entities_))
      .credit(makeCreditPass(*rng_, *entities_, creditLifecycle_))
      .familyScenario(familyScenario_)
      .router(infra_->router);

  return builder;
}

std::vector<Transaction> TransferStage::replayStream(
    const PrimaryAccounts &primaryAccounts,
    legit_ledger::TransfersPayload &legitPayload) const {
  std::vector<Transaction> stream = std::move(legitPayload.replaySortedTxns);

  ::PhantomLedger::transactions::Factory txf{*rng_, &infra_->router,
                                             &infra_->ringInfra};

  insurance::Population insPop{.primaryAccounts = &primaryAccounts};

  auto premiumTxns =
      insurance::premiums(window_, *rng_, txf, entities_->portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), premiumTxns);

  ::PhantomLedger::random::RngFactory claimsFactory{seed_};
  insurance::ClaimScheduler claimScheduler{claimRates_, *rng_, txf,
                                           claimsFactory};
  auto claimTxns =
      claimScheduler.generate(window_, entities_->portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), claimTxns);

  obligations::Population oblPop{.primaryAccounts = &primaryAccounts};
  obligations::Scheduler obligationsScheduler{*rng_, txf};
  auto obligationTxns =
      obligationsScheduler.generate(entities_->portfolios,
                                    ::PhantomLedger::time::HalfOpenInterval{
                                        .start = window_.start,
                                        .endExcl = window_.endExcl(),
                                    },
                                    oblPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), obligationTxns);

  return stream;
}

fraud::InjectionOutput TransferStage::injectFraud(
    std::span<const Transaction> draftTxns,
    const legit_ledger::TransfersPayload &legitPayload) const {
  fraud::Injector injector{
      fraud::Injector::Services{
          .rng = rng_,
          .router = &infra_->router,
          .ringInfra = &infra_->ringInfra,
      },
      fraudRules_,
  };

  return injector.inject(
      fraud::Injector::FraudPopulation{
          .profile = fraudProfile_,
          .window = window_,
          .topology = &entities_->people.topology,
          .accounts = &entities_->accounts.registry,
          .ownership = &entities_->accounts.ownership,
          .baseTxns = draftTxns,
      },
      fraud::Injector::LegitCounterparties{
          .billerAccounts = std::span<const ::PhantomLedger::entity::Key>(
              legitPayload.billerAccounts.data(),
              legitPayload.billerAccounts.size()),
          .employers = std::span<const ::PhantomLedger::entity::Key>(
              legitPayload.employers.data(), legitPayload.employers.size()),
      });
}

::PhantomLedger::pipeline::Transfers TransferStage::build() const {
  validate::require(recurringIncome_);
  validateHubFraction(hubFraction_);

  auto builder = makeLegitBuilder();
  legit_ledger::TransfersPayload legitPayload = builder.build();

  if (legitPayload.initialBook == nullptr) {
    throw std::runtime_error(
        "transfers::TransferStage::build: legit builder produced no initial "
        "book");
  }

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities_->accounts.lookup, legitPayload.candidateTxns);

  const auto primaryAccountsByPerson = primaryAccounts();
  auto replaySortedStream = replayStream(primaryAccountsByPerson, legitPayload);

  auto preReplay =
      runPreFraudReplay(*legitPayload.initialBook, *rng_, replayRules_,
                        std::move(replaySortedStream));

  const std::span<const Transaction> draftView{preReplay.draftTxns.data(),
                                               preReplay.draftTxns.size()};
  auto fraudOut = injectFraud(draftView, legitPayload);

  auto mergedTxns = std::move(fraudOut.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));

  auto postReplay = runPostFraudReplay(*rng_, *legitPayload.initialBook,
                                       replayRules_, std::move(mergedTxns));

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities_->accounts.lookup, postReplay.finalTxns);

  ::PhantomLedger::pipeline::Transfers out{};
  out.legit = std::move(legitPayload);
  out.fraud.injectedCount = fraudOut.injectedCount;
  out.draftTxns = std::move(preReplay.draftTxns);
  out.finalTxns = std::move(postReplay.finalTxns);
  out.finalBook = std::move(postReplay.finalBook);
  out.dropCounts = std::move(preReplay.dropCounts);
  out.dropCountsByChannel = std::move(preReplay.dropCountsByChannel);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
