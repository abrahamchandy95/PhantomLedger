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
makeRecurringIncomeRules(const Inputs &in) {
  return ::PhantomLedger::inflows::RecurringIncomeRules{
      .employment = in.income.employment,
      .lease = in.income.lease,
      .salaryPaidFraction = in.income.salaryPaidFraction,
      .rentPaidFraction = in.income.rentPaidFraction,
  };
}

[[nodiscard]] blueprints::PlanRequest
makePlanRequest(::PhantomLedger::random::Rng &rng,
                const ::PhantomLedger::pipeline::Entities &entities,
                const Inputs &in) {
  return blueprints::PlanRequest{
      .window = in.window,
      .rng = &rng,
      .population =
          blueprints::PopulationTuning{
              .count = entities.people.roster.count,
              .seed = in.seed,
              .hubFraction = in.hubFraction,
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
                       const Inputs &in) {
  return legit_ledger::BalanceBookRequest{
      .window = in.window,
      .rng = &rng,
      .accounts = &entities.accounts.registry,
      .accountsLookup = &entities.accounts.lookup,
      .ownership = &entities.accounts.ownership,
      .rules = in.clearing.balanceRules,
      .portfolios = &entities.portfolios,
      .creditCards = &entities.creditCards,
  };
}

[[nodiscard]] legit_ledger::passes::IncomePassRequest
makeIncomePassRequest(::PhantomLedger::random::Rng &rng,
                      const ::PhantomLedger::pipeline::Entities &entities,
                      const Inputs &in) {
  return legit_ledger::passes::IncomePassRequest{
      .rng = &rng,
      .accounts = &entities.accounts.registry,
      .ownership = &entities.accounts.ownership,
      .revenueCounterparties = &entities.counterparties,
      .recurring = makeRecurringIncomeRules(in),
      .retirement = &in.government.retirement,
      .disability = &in.government.disability,
  };
}

[[nodiscard]] legit_ledger::passes::RoutinePassRequest
makeRoutinePassRequest(::PhantomLedger::random::Rng &rng,
                       const ::PhantomLedger::pipeline::Entities &entities,
                       const Inputs &in) {
  return legit_ledger::passes::RoutinePassRequest{
      .rng = &rng,
      .accountsLookup = &entities.accounts.lookup,
      .merchants = &entities.merchants.catalog,
      .portfolios = &entities.portfolios,
      .creditCards = &entities.creditCards,
      .recurring = makeRecurringIncomeRules(in),
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
                      const Inputs &in) {
  return legit_ledger::passes::CreditPassRequest{
      .rng = &rng,
      .cards = &entities.creditCards,
      .lifecycle = in.creditCards.lifecycle,
  };
}

[[nodiscard]] legit_ledger::LegitTransferRequest
makeLegitTransferRequest(::PhantomLedger::random::Rng &rng,
                         const ::PhantomLedger::pipeline::Entities &entities,
                         const ::PhantomLedger::pipeline::Infra &infra,
                         const Inputs &in) {
  return legit_ledger::LegitTransferRequest{
      .plan = makePlanRequest(rng, entities, in),
      .balanceBook = makeBalanceBookRequest(rng, entities, in),
      .income = makeIncomePassRequest(rng, entities, in),
      .routines = makeRoutinePassRequest(rng, entities, in),
      .family = makeFamilyPassRequest(entities),
      .credit = makeCreditPassRequest(rng, entities, in),
      .famGraphCfg = in.family.graph,
      .familyTransfers = in.family.transfers,
      .router = &infra.router,
  };
}

[[nodiscard]] std::vector<Transaction> assembleReplayStream(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::pipeline::Infra &infra, const Inputs &in,
    const std::unordered_map<::PhantomLedger::entity::PersonId,
                             ::PhantomLedger::entity::Key> &primaryAccounts,
    legit_ledger::TransfersPayload &legitPayload) {
  std::vector<Transaction> stream = std::move(legitPayload.replaySortedTxns);

  ::PhantomLedger::transactions::Factory txf{rng, &infra.router,
                                             &infra.ringInfra};

  insurance::Population insPop{.primaryAccounts = &primaryAccounts};

  auto premiumTxns =
      insurance::premiums(in.window, rng, txf, entities.portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), premiumTxns);

  ::PhantomLedger::random::RngFactory claimsFactory{in.seed};
  auto claimTxns =
      insurance::claims(in.insurance.claimRates, in.window, rng, txf,
                        claimsFactory, entities.portfolios, insPop);
  stream = legit_ledger::mergeReplaySorted(std::move(stream), claimTxns);

  obligations::Population oblPop{.primaryAccounts = &primaryAccounts};
  auto obligationTxns =
      obligations::scheduledPayments(entities.portfolios, in.window.start,
                                     in.window.endExcl(), oblPop, rng, txf);
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
                  legit_ledger::ReplayPolicy policy,
                  std::vector<Transaction> sorted) {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, policy, /*emitLiquidityEvents=*/true);

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
                  const ::PhantomLedger::pipeline::Infra &infra,
                  const Inputs &in, std::span<const Transaction> draftTxns,
                  const legit_ledger::TransfersPayload &legitPayload) {
  fraud::InjectionInput req{};

  req.scenario.fraudCfg = nullptr;
  req.scenario.window = in.window;
  req.scenario.people = &entities.people.roster;
  req.scenario.topology = &entities.people.topology;
  req.scenario.accounts = &entities.accounts.registry;
  req.scenario.accountsLookup = &entities.accounts.lookup;
  req.scenario.ownership = &entities.accounts.ownership;
  req.scenario.baseTxns = draftTxns;

  req.runtime.rng = &rng;
  req.runtime.router = &infra.router;
  req.runtime.ringInfra = &infra.ringInfra;

  req.counterparties.billerAccounts = legitPayload.billerAccounts;
  req.counterparties.employers = legitPayload.employers;

  req.params = in.fraud.params;

  return fraud::inject(req);
}

struct PostReplayResult {
  std::vector<Transaction> finalTxns;
  std::unique_ptr<::PhantomLedger::clearing::Ledger> finalBook;
};

[[nodiscard]] PostReplayResult
runPostFraudReplay(::PhantomLedger::random::Rng &rng,
                   const ::PhantomLedger::clearing::Ledger &initialBook,
                   legit_ledger::ReplayPolicy policy,
                   std::vector<Transaction> merged) {
  auto bookCopy =
      std::make_unique<::PhantomLedger::clearing::Ledger>(initialBook);

  legit_ledger::ChronoReplayAccumulator accumulator(
      bookCopy.get(), &rng, policy, /*emitLiquidityEvents=*/false);

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
      const ::PhantomLedger::pipeline::Infra &infra, const Inputs &in) {
  primitives::validate::require(in);

  auto legitRequest = makeLegitTransferRequest(rng, entities, infra, in);

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

  auto replaySortedStream = assembleReplayStream(rng, entities, infra, in,
                                                 primaryAccounts, legitPayload);

  auto preReplay =
      runPreFraudReplay(*legitPayload.initialBook, rng, in.replay.preFraud,
                        std::move(replaySortedStream));

  auto injection = runFraudInjection(rng, entities, infra, in,
                                     preReplay.draftTxns, legitPayload);

  auto mergedTxns = std::move(injection.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));

  auto postReplay =
      runPostFraudReplay(rng, *legitPayload.initialBook, in.replay.preFraud,
                         std::move(mergedTxns));

  ::PhantomLedger::pipeline::validateTransactionAccounts(
      entities.accounts.lookup, postReplay.finalTxns);

  ::PhantomLedger::pipeline::Transfers out{};
  out.legit = std::move(legitPayload);
  out.fraud.injectedCount = injection.injectedCount;
  out.draftTxns = std::move(preReplay.draftTxns);
  out.finalTxns = std::move(postReplay.finalTxns);
  out.finalBook = std::move(postReplay.finalBook);
  out.dropCounts = std::move(preReplay.dropCounts);
  out.dropCountsByChannel = std::move(preReplay.dropCountsByChannel);

  return out;
}

} // namespace PhantomLedger::pipeline::stages::transfers
