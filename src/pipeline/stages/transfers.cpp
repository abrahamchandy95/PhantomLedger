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

[[nodiscard]] std::unordered_map<::PhantomLedger::entity::PersonId,
                                 ::PhantomLedger::entity::Key>
buildPrimaryAccountsMap(
    const ::PhantomLedger::entity::account::Registry &registry) {
  std::unordered_map<::PhantomLedger::entity::PersonId,
                     ::PhantomLedger::entity::Key>
      out;
  out.reserve(registry.records.size());

  for (const auto &record : registry.records) {
    if (record.owner == ::PhantomLedger::entity::invalidPerson) {
      continue;
    }

    out.try_emplace(record.owner, record.id);
  }

  return out;
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

[[nodiscard]] blueprints::PlanRequest
makePlanRequest(::PhantomLedger::random::Rng &rng,
                const ::PhantomLedger::pipeline::Entities &entities,
                RunScope run, PopulationShape population) {
  return blueprints::PlanRequest{
      .window = run.window,
      .rng = &rng,
      .population =
          blueprints::PopulationTuning{
              .count = entities.people.roster.count,
              .seed = run.seed,
              .hubFraction = population.hubFraction,
          },
      .census =
          blueprints::CensusSource{
              .accounts = &entities.accounts.registry,
              .ownership = &entities.accounts.ownership,
          },
      .counterparties =
          blueprints::CounterpartySource{
              .directory = &entities.counterparties,
              .landlords = &entities.landlords.roster,
          },
      .personas =
          blueprints::PersonaSource{
              .pack = &entities.personas,
          },
  };
}

[[nodiscard]] legit_ledger::BalanceBookRequest
makeBalanceBookRequest(::PhantomLedger::random::Rng &rng,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       RunScope run, const BalanceBookRules &balanceBook) {
  return legit_ledger::BalanceBookRequest{
      .window = run.window,
      .rng = &rng,
      .accounts = &entities.accounts.registry,
      .accountsLookup = &entities.accounts.lookup,
      .ownership = &entities.accounts.ownership,
      .rules = balanceBook.balanceRules,
      .portfolios = &entities.portfolios,
      .creditCards = &entities.creditCards,
  };
}

[[nodiscard]] legit_ledger::passes::IncomePassRequest
makeIncomePassRequest(::PhantomLedger::random::Rng &rng,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const RecurringIncome &income,
                      const GovernmentPrograms &government) {
  return legit_ledger::passes::IncomePassRequest{
      .rng = &rng,
      .accounts = &entities.accounts.registry,
      .ownership = &entities.accounts.ownership,
      .revenueCounterparties = &entities.counterparties,
      .recurring = makeRecurringIncomeRules(income),
      .retirement = &government.retirement,
      .disability = &government.disability,
  };
}

[[nodiscard]] legit_ledger::passes::RoutinePassRequest
makeRoutinePassRequest(::PhantomLedger::random::Rng &rng,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const RecurringIncome &income) {
  return legit_ledger::passes::RoutinePassRequest{
      .rng = &rng,
      .accountsLookup = &entities.accounts.lookup,
      .merchants = &entities.merchants.catalog,
      .portfolios = &entities.portfolios,
      .creditCards = &entities.creditCards,
      .recurring = makeRecurringIncomeRules(income),
  };
}

[[nodiscard]] legit_ledger::passes::FamilyPassRequest
makeFamilyPassRequest(const ::PhantomLedger::pipeline::Entities &entities) {
  return legit_ledger::passes::FamilyPassRequest{
      .accounts = &entities.accounts.registry,
      .ownership = &entities.accounts.ownership,
      .merchants = &entities.merchants.catalog,
  };
}

[[nodiscard]] legit_ledger::passes::CreditPassRequest
makeCreditPassRequest(::PhantomLedger::random::Rng &rng,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const CreditCardLifecycle &creditCards) {
  return legit_ledger::passes::CreditPassRequest{
      .rng = &rng,
      .cards = &entities.creditCards,
      .lifecycle = creditCards.lifecycle,
  };
}

[[nodiscard]] legit_ledger::LegitTransferRequest makeLegitTransferRequest(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::pipeline::Infra &infra, RunScope run,
    const RecurringIncome &income, const BalanceBookRules &balanceBook,
    const CreditCardLifecycle &creditCards, const FamilyPrograms &family,
    const GovernmentPrograms &government, PopulationShape population) {
  return legit_ledger::LegitTransferRequest{
      .plan = makePlanRequest(rng, entities, run, population),
      .balanceBook = makeBalanceBookRequest(rng, entities, run, balanceBook),
      .income = makeIncomePassRequest(rng, entities, income, government),
      .routines = makeRoutinePassRequest(rng, entities, income),
      .family = makeFamilyPassRequest(entities),
      .credit = makeCreditPassRequest(rng, entities, creditCards),
      .familyHouseholds = family.households,
      .familyDependents = family.dependents,
      .retireeSupport = family.retireeSupport,
      .familyTransfers = family.transfers,
      .router = &infra.router,
  };
}

[[nodiscard]] std::vector<Transaction> assembleReplayStream(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::pipeline::Infra &infra, RunScope run,
    const InsuranceClaims &insuranceClaims,
    const std::unordered_map<::PhantomLedger::entity::PersonId,
                             ::PhantomLedger::entity::Key> &primaryAccounts,
    legit_ledger::TransfersPayload &legitPayload) {
  std::vector<Transaction> stream = std::move(legitPayload.replaySortedTxns);

  ::PhantomLedger::transactions::Factory txf{rng, &infra.router,
                                             &infra.ringInfra};

  insurance::Population insPop{.primaryAccounts = &primaryAccounts};

  auto premiumTxns =
      insurance::premiums(run.window, rng, txf, entities.portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), premiumTxns);

  ::PhantomLedger::random::RngFactory claimsFactory{run.seed};
  auto claimTxns =
      insurance::claims(insuranceClaims.claimRates, run.window, rng, txf,
                        claimsFactory, entities.portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), claimTxns);

  obligations::Population oblPop{.primaryAccounts = &primaryAccounts};
  auto obligationTxns =
      obligations::scheduledPayments(entities.portfolios, run.window.start,
                                     run.window.endExcl(), oblPop, rng, txf);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), obligationTxns);

  return stream;
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

[[nodiscard]] fraud::InjectionOutput
runFraudInjection(::PhantomLedger::random::Rng &rng,
                  const ::PhantomLedger::pipeline::Entities &entities,
                  const ::PhantomLedger::pipeline::Infra &infra, RunScope run,
                  const FraudInjection &fraudInjection,
                  std::span<const Transaction> draftTxns,
                  const legit_ledger::TransfersPayload &legitPayload) {
  fraud::Injector injector{
      fraud::Injector::Services{
          .rng = &rng,
          .router = &infra.router,
          .ringInfra = &infra.ringInfra,
      },
      fraudInjection.rules,
  };

  return injector.inject(
      fraud::Injector::FraudPopulation{
          .profile = fraudInjection.profile,
          .window = run.window,
          .topology = &entities.people.topology,
          .accounts = &entities.accounts.registry,
          .ownership = &entities.accounts.ownership,
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

::PhantomLedger::pipeline::Transfers
build(::PhantomLedger::random::Rng &rng,
      const ::PhantomLedger::pipeline::Entities &entities,
      const ::PhantomLedger::pipeline::Infra &infra, RunScope run,
      const RecurringIncome &income, const BalanceBookRules &balanceBook,
      const CreditCardLifecycle &creditCards, const FamilyPrograms &family,
      const GovernmentPrograms &government,
      const InsuranceClaims &insuranceClaims, const LedgerReplay &replay,
      const FraudInjection &fraudInjection, PopulationShape population) {
  primitives::validate::require(income);
  primitives::validate::require(population);

  auto legitRequest =
      makeLegitTransferRequest(rng, entities, infra, run, income, balanceBook,
                               creditCards, family, government, population);

  legit_ledger::LegitTransferBuilder builder{
      .request = &legitRequest,
  };

  legit_ledger::TransfersPayload legitPayload = builder.build();

  if (legitPayload.initialBook == nullptr) {
    throw std::runtime_error(
        "transfers::build: legit builder produced no initial book");
  }

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities.accounts.lookup, legitPayload.candidateTxns);

  const auto primaryAccounts =
      buildPrimaryAccountsMap(entities.accounts.registry);

  auto replaySortedStream =
      assembleReplayStream(rng, entities, infra, run, insuranceClaims,
                           primaryAccounts, legitPayload);

  auto preReplay =
      runPreFraudReplay(*legitPayload.initialBook, rng, replay.preFraud,
                        std::move(replaySortedStream));

  auto fraudOut = runFraudInjection(rng, entities, infra, run, fraudInjection,
                                    preReplay.draftTxns, legitPayload);

  auto mergedTxns = std::move(fraudOut.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));

  auto postReplay = runPostFraudReplay(rng, *legitPayload.initialBook,
                                       replay.preFraud, std::move(mergedTxns));

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities.accounts.lookup, postReplay.finalTxns);

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
