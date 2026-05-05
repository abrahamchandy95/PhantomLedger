#include "phantomledger/pipeline/stages/transfers.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/inflows/types.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
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

using Transaction = ::PhantomLedger::transactions::Transaction;
using ChannelReasonKey = ::PhantomLedger::pipeline::Transfers::ChannelReasonKey;
using ChannelReasonHash =
    ::PhantomLedger::pipeline::Transfers::ChannelReasonHash;

constexpr double kFraudHeadroomFraction = 0.05;

[[nodiscard]] std::size_t fraudMergedCapacity(std::size_t legitSize) noexcept {
  return legitSize + static_cast<std::size_t>(static_cast<double>(legitSize) *
                                              kFraudHeadroomFraction);
}

[[nodiscard]] ::PhantomLedger::inflows::RecurringIncomeRules
makeRecurringIncomeRules(const RecurringIncome &income) {
  return ::PhantomLedger::inflows::RecurringIncomeRules{
      .employment = income.employment,
      .lease = income.lease,
      .salaryPaidFraction = income.salaryPaidFraction,
      .rentPaidFraction = income.rentPaidFraction,
  };
}

[[nodiscard]] blueprints::LegitTimeframe
makeLegitTimeframe(RunScope run) noexcept {
  return blueprints::LegitTimeframe{
      .window = run.window,
      .seed = run.seed,
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
                 PopulationShape population) noexcept {
  return blueprints::HubSelectionRules{
      .populationCount = entities.people.roster.count,
      .fraction = population.hubFraction,
  };
}

[[nodiscard]] legit_ledger::OpeningBook
makeOpeningBook(::PhantomLedger::random::Rng &rng,
                const ::PhantomLedger::pipeline::Entities &entities,
                OpeningBookProtections openingBook) noexcept {
  return legit_ledger::OpeningBook{
      rng,
      legit_ledger::OpeningBook::Accounts{
          .registry = &entities.accounts.registry,
          .lookup = &entities.accounts.lookup,
          .ownership = &entities.accounts.ownership,
      },
      legit_ledger::OpeningBook::Protections{
          .balanceRules = openingBook.balanceRules,
          .portfolios = &entities.portfolios,
          .creditCards = &entities.creditCards,
      },
  };
}

[[nodiscard]] legit_ledger::passes::IncomePass
makeIncomePass(::PhantomLedger::random::Rng &rng,
               const ::PhantomLedger::pipeline::Entities &entities,
               const RecurringIncome &income,
               const GovernmentPrograms &government) {
  return legit_ledger::passes::IncomePass{
      &rng,
      legit_ledger::passes::AccountAccess{
          .registry = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
      },
      &entities.counterparties,
      makeRecurringIncomeRules(income),
      legit_ledger::passes::GovernmentBenefits{
          .retirement = &government.retirement,
          .disability = &government.disability,
      },
  };
}

[[nodiscard]] legit_ledger::passes::RoutinePass
makeRoutinePass(::PhantomLedger::random::Rng &rng,
                const ::PhantomLedger::pipeline::Entities &entities,
                const RecurringIncome &income) {
  return legit_ledger::passes::RoutinePass{
      &rng,
      legit_ledger::passes::RoutineResources{
          .accountsLookup = &entities.accounts.lookup,
          .merchants = &entities.merchants.catalog,
          .portfolios = &entities.portfolios,
          .creditCards = &entities.creditCards,
      },
      makeRecurringIncomeRules(income),
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
               const CreditCardLifecycle &creditCards) {
  return legit_ledger::passes::CreditLifecyclePass{
      &rng,
      &entities.creditCards,
      creditCards.lifecycle,
  };
}

[[nodiscard]] legit_ledger::LegitTransferBuilder::FamilyPrograms
makeLegitFamilyPrograms(const FamilyPrograms &family) noexcept {
  return legit_ledger::LegitTransferBuilder::FamilyPrograms{
      .households = family.households,
      .dependents = family.dependents,
      .retireeSupport = family.retireeSupport,
      .transfers = family.transfers,
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

TransferStage &TransferStage::scope(RunScope value) noexcept {
  run_ = value;
  return *this;
}

TransferStage &TransferStage::income(const RecurringIncome &value) {
  income_ = value;
  return *this;
}

TransferStage &
TransferStage::openingBook(OpeningBookProtections value) noexcept {
  openingBook_ = value;
  return *this;
}

TransferStage &TransferStage::creditCards(CreditCardLifecycle value) noexcept {
  creditCards_ = value;
  return *this;
}

TransferStage &TransferStage::family(FamilyPrograms value) noexcept {
  family_ = value;
  return *this;
}

TransferStage &TransferStage::government(const GovernmentPrograms &value) {
  government_ = value;
  return *this;
}

TransferStage &TransferStage::insurance(InsuranceClaims value) noexcept {
  insurance_ = value;
  return *this;
}

TransferStage &TransferStage::replay(LedgerReplay value) noexcept {
  replay_ = value;
  return *this;
}

TransferStage &TransferStage::fraud(FraudInjection value) noexcept {
  fraud_ = value;
  return *this;
}

TransferStage &TransferStage::population(PopulationShape value) noexcept {
  population_ = value;
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
      makeLegitTimeframe(run_),
      makeAccountCensus(*entities_),
      makeOpeningBook(*rng_, *entities_, openingBook_),
  };

  builder.counterparties(makeCounterpartyPools(*entities_))
      .personas(makePersonaCatalog(*entities_))
      .hubSelection(makeHubSelection(*entities_, population_))
      .income(makeIncomePass(*rng_, *entities_, income_, government_))
      .routines(makeRoutinePass(*rng_, *entities_, income_))
      .family(makeFamilyPass(*entities_))
      .credit(makeCreditPass(*rng_, *entities_, creditCards_))
      .familyPrograms(makeLegitFamilyPrograms(family_))
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

  auto premiumTxns = insurance::premiums(run_.window, *rng_, txf,
                                         entities_->portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), premiumTxns);

  ::PhantomLedger::random::RngFactory claimsFactory{run_.seed};
  auto claimTxns =
      insurance::claims(insurance_.claimRates, run_.window, *rng_, txf,
                        claimsFactory, entities_->portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), claimTxns);

  obligations::Population oblPop{.primaryAccounts = &primaryAccounts};
  obligations::Scheduler obligationsScheduler{*rng_, txf};
  auto obligationTxns =
      obligationsScheduler.generate(entities_->portfolios,
                                    ::PhantomLedger::time::HalfOpenInterval{
                                        .start = run_.window.start,
                                        .endExcl = run_.window.endExcl(),
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
      fraud_.rules,
  };

  return injector.inject(
      fraud::Injector::FraudPopulation{
          .profile = fraud_.profile,
          .window = run_.window,
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
  primitives::validate::require(income_);
  primitives::validate::require(population_);

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
      runPreFraudReplay(*legitPayload.initialBook, *rng_, replay_.preFraud,
                        std::move(replaySortedStream));

  const std::span<const Transaction> draftView{preReplay.draftTxns.data(),
                                               preReplay.draftTxns.size()};
  auto fraudOut = injectFraud(draftView, legitPayload);

  auto mergedTxns = std::move(fraudOut.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));

  auto postReplay = runPostFraudReplay(*rng_, *legitPayload.initialBook,
                                       replay_.preFraud, std::move(mergedTxns));

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
