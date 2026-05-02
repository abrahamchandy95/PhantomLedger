#include "phantomledger/pipeline/stages/transfers.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/pipeline/invariants.hpp"
#include "phantomledger/primitives/validate/checks.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/fraud/injector.hpp"
#include "phantomledger/transfers/insurance/claims.hpp"
#include "phantomledger/transfers/insurance/premiums.hpp"
#include "phantomledger/transfers/legit/blueprints/models.hpp"
#include "phantomledger/transfers/legit/ledger/builder.hpp"
#include "phantomledger/transfers/legit/ledger/posting.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/obligations/schedule.hpp"

#include <cstddef>
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

[[nodiscard]] blueprints::GovernmentPrograms
makeGovernmentPrograms(const Inputs &in) {
  return blueprints::GovernmentPrograms{
      .retirement = in.government.retirement,
      .disability = in.government.disability,
  };
}

[[nodiscard]] blueprints::IncomeSpec makeIncomeSpec(const Inputs &in) {
  return blueprints::IncomeSpec{
      .employment = in.income.employment,
      .lease = in.income.lease,
      .salaryPaidFraction = in.income.salaryPaidFraction,
      .rentPaidFraction = in.income.rentPaidFraction,
  };
}

[[nodiscard]] blueprints::CreditCardSpec makeCreditCardSpec(const Inputs &in) {
  return blueprints::CreditCardSpec{
      .issuance = nullptr,
      .terms = in.creditCards.terms,
      .habits = in.creditCards.habits,
  };
}

[[nodiscard]] blueprints::Blueprint
makeBlueprint(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::pipeline::Entities &entities,
              const ::PhantomLedger::pipeline::Infra &infra, const Inputs &in,
              blueprints::GovernmentPrograms &governmentStore) {
  governmentStore = makeGovernmentPrograms(in);

  blueprints::Blueprint bp{};

  bp.timeline.window = in.window;
  bp.timeline.rng = &rng;

  bp.network.accounts = &entities.accounts.registry;
  bp.network.accountsLookup = &entities.accounts.lookup;
  bp.network.ownership = &entities.accounts.ownership;
  bp.network.merchants = &entities.merchants.catalog;
  bp.network.landlords = &entities.landlords.roster;
  bp.network.landlordsIndex = &entities.landlords.index;
  bp.network.portfolios = &entities.portfolios;

  bp.macro.population.count = entities.people.roster.count;
  bp.macro.population.seed = in.seed;
  bp.macro.hubSelection.fraction = in.hubFraction;
  bp.macro.government = &governmentStore;

  bp.income = makeIncomeSpec(in);
  bp.clearing.balances = in.clearing.balanceRules;
  bp.creditCards = makeCreditCardSpec(in);

  bp.overrides.infra = &infra.router;
  bp.overrides.personas = &entities.personas;
  bp.overrides.counterparties = &entities.counterparties;

  bp.creditCardState.cards = &entities.creditCards;

  primitives::validate::require(bp);
  return bp;
}

[[nodiscard]] std::vector<Transaction> assembleReplayStream(
    ::PhantomLedger::random::Rng &rng,
    const ::PhantomLedger::pipeline::Entities &entities,
    const ::PhantomLedger::pipeline::Infra &infra, const Inputs &in,
    const std::unordered_map<::PhantomLedger::entity::PersonId,
                             ::PhantomLedger::entity::Key> &primaryAccounts,
    blueprints::TransfersPayload &legitPayload) {
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
                  const blueprints::TransfersPayload &legitPayload) {
  fraud::InjectionInput req{};

  req.scenario.fraudCfg = nullptr; // builder default; entities synth pack
                                   // owns the canonical values
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

  blueprints::GovernmentPrograms governmentStore{};
  auto blueprint = makeBlueprint(rng, entities, infra, in, governmentStore);

  legit_ledger::LegitTransferBuilder builder{
      .request = &blueprint,
      .famGraphCfg = in.family.graph,
      .familyTransfers = in.family.transfers,
  };

  blueprints::TransfersPayload legitPayload = builder.build();

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
