#include "phantomledger/pipeline/stages/transfers.hpp"

#include "phantomledger/entropy/random/factory.hpp"
#include "phantomledger/pipeline/invariants.hpp"
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

[[nodiscard]] blueprints::Blueprint
makeBlueprint(::PhantomLedger::random::Rng &rng,
              const ::PhantomLedger::pipeline::Entities &entities,
              const ::PhantomLedger::pipeline::Infra &infra, const Inputs &in,
              blueprints::GovernmentPrograms &govStore /*owned by caller*/,
              blueprints::CreditCardProfile &ccStore /*owned by caller*/) {
  govStore.retirement = in.retirement;
  govStore.disability = in.disability;

  ccStore.terms = in.ccTerms;
  ccStore.habits = in.ccHabits;

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
  bp.macro.government = &govStore;

  bp.specs.recurringPolicy = in.recurringPolicy;
  bp.specs.balances = in.balanceRules;
  bp.specs.ccProfile = ccStore;

  bp.overrides.infra = &infra.router;
  bp.overrides.personas = &entities.personas;
  bp.overrides.counterpartyPools = &entities.counterpartyPools;

  bp.ccState.cards = &entities.creditCards;

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
      insurance::claims(in.claimRates, in.window, rng, txf, claimsFactory,
                        entities.portfolios, insPop);
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

  // Runtime — RNG + infra views.
  req.runtime.rng = &rng;
  req.runtime.router = &infra.router;
  req.runtime.ringInfra = &infra.ringInfra;

  req.counterparties.billerAccounts = legitPayload.billerAccounts;
  req.counterparties.employers = legitPayload.employers;

  req.params = in.fraudParams;

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

  blueprints::GovernmentPrograms govStore{};
  blueprints::CreditCardProfile ccStore{};
  auto blueprint = makeBlueprint(rng, entities, infra, in, govStore, ccStore);

  legit_ledger::LegitTransferBuilder builder{
      .request = &blueprint,
      .famGraphCfg = in.familyGraphCfg,
      .familyTransfers = in.familyTransfers,
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
      runPreFraudReplay(*legitPayload.initialBook, rng, in.preReplayPolicy,
                        std::move(replaySortedStream));

  auto injection = runFraudInjection(rng, entities, infra, in,
                                     preReplay.draftTxns, legitPayload);

  auto mergedTxns = std::move(injection.txns);
  mergedTxns.reserve(fraudMergedCapacity(mergedTxns.size()));

  auto postReplay =
      runPostFraudReplay(rng, *legitPayload.initialBook, in.preReplayPolicy,
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
