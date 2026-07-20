#include "phantomledger/transfers/legit/ledger/builder.hpp"

#include "phantomledger/transfers/legit/ledger/memlog.hpp"

#include "phantomledger/entities/infra/router.hpp"
#include "phantomledger/primitives/random/factory.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace PhantomLedger::transfers::legit::ledger {

namespace {

namespace relatives = ::PhantomLedger::transfers::legit::routines::relatives;
namespace family_rt = ::PhantomLedger::transfers::legit::routines::family;

[[nodiscard]] relatives::FamilyLedgerSources
familySourcesFrom(const passes::FamilyPass &pass) noexcept {
  const auto accounts = pass.accounts();

  return relatives::FamilyLedgerSources{
      .accounts = accounts.registry,
      .ownership = accounts.ownership,
      .educationMerchants = pass.merchants(),
  };
}

} // namespace

LegitTransferBuilder::LegitTransferBuilder(random::Rng &rng,
                                           blueprints::LegitTimeframe timeframe,
                                           blueprints::AccountCensus census,
                                           OpeningBook openingBook) noexcept
    : rng_(&rng), timeframe_(timeframe), census_(census),
      openingBook_(std::move(openingBook)) {}

LegitTransferBuilder &LegitTransferBuilder::rng(random::Rng &value) noexcept {
  rng_ = &value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::timeframe(blueprints::LegitTimeframe value) noexcept {
  timeframe_ = value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::census(blueprints::AccountCensus value) noexcept {
  census_ = value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::counterparties(
    blueprints::CounterpartyPools value) noexcept {
  counterparties_ = value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::personas(blueprints::PersonaCatalog value) noexcept {
  personas_ = value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::hubSelection(
    blueprints::HubSelectionRules value) noexcept {
  hubSelection_ = value;
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::openingBook(OpeningBook value) noexcept {
  openingBook_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::income(passes::IncomePass value) noexcept {
  income_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::routines(passes::RoutinePass value) noexcept {
  routines_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::family(passes::FamilyPass value) noexcept {
  family_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::credit(passes::CreditLifecyclePass value) noexcept {
  credit_ = std::move(value);
  return *this;
}

LegitTransferBuilder &
LegitTransferBuilder::familyScenario(FamilyTransferScenario value) noexcept {
  familyScenario_ = value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::router(
    const ::PhantomLedger::infra::Router &value) noexcept {
  router_ = &value;
  return *this;
}

LegitTransferBuilder &LegitTransferBuilder::router(
    const ::PhantomLedger::infra::Router *value) noexcept {
  router_ = value;
  return *this;
}

const entity::account::Registry *
LegitTransferBuilder::accounts() const noexcept {
  return census_.accounts;
}

std::vector<transactions::Transaction> LegitTransferBuilder::buildFamilyRows(
    const blueprints::LegitBlueprint &plan,
    const ::PhantomLedger::infra::Router *familyRouter) const {
  // Family rows draw amounts AND device/IP routing from dedicated
  // deterministic lanes against the supplied pristine router copy, so
  // family generation neither consumes shared-stream draws nor perturbs
  // the sticky routing state that income, routines and spending share —
  // the same order-decoupling the product schedule uses. A windowed
  // composition can therefore generate identical family rows at any
  // sequence point.
  const random::RngFactory familyRngFactory{plan.seed()};
  auto familyRoutingRng = familyRngFactory.rng({"family", "routing"});
  const transactions::Factory familyTxf(familyRoutingRng, familyRouter);

  return relatives::generateFamilyTxns(
      plan, familySourcesFrom(family_),
      family_rt::TransferEmission{familyRngFactory, familyTxf},
      familyScenario_);
}

LegitTransferResult LegitTransferBuilder::build() const {
  if (rng_ == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.build() requires a non-null rng");
  }

  const auto *accountRegistry = accounts();
  if (accountRegistry == nullptr || accountRegistry->records.empty()) {
    return LegitTransferResult{};
  }

  // Pristine router copy for family routing, taken before any pass routes
  // through the shared router; see buildFamilyRows().
  const ::PhantomLedger::infra::Router familyRouter =
      router_ != nullptr ? *router_ : ::PhantomLedger::infra::Router{};

  auto plan = blueprints::buildLegitBlueprint(timeframe_, census_);
  plan.addCounterparties(*rng_, census_, counterparties_, hubSelection_)
      .addPersonas(*rng_, timeframe_, personas_);

  auto initialBook = openingBook_.build(plan);
  memlog::logPlain("openingBook");

  TxnStreams streams;
  ScreenBook screen{initialBook.get()};

  const transactions::Factory txf(*rng_, router_);

  passes::addIncome(income_, plan, txf, streams);
  memlog::log("income", streams);

  if (census_.ownership != nullptr && census_.accounts != nullptr) {
    auto routinePass = routines_;
    routinePass.accounts(passes::AccountAccess{
        .registry = census_.accounts,
        .ownership = census_.ownership,
    });
    routinePass.txf(txf);
    passes::addRoutines(routinePass, plan, streams, screen);
  }

  memlog::log("routines:done", streams);

  streams.add(
      buildFamilyRows(plan, router_ != nullptr ? &familyRouter : nullptr));

  LegitTransferResult result;
  auto counterparties = std::move(plan).takeCounterparties();
  result.counterparties.hubAccounts = std::move(counterparties.hubAccounts);
  result.counterparties.billerAccounts =
      std::move(counterparties.billerAccounts);
  result.counterparties.employers = std::move(counterparties.employers);
  result.openingBook.initialBook = std::move(initialBook);
  memlog::log("family", streams);
  result.txns.replaySortedTxns = streams.takeReplayReady();

  return result;
}

WindowedPrologue LegitTransferBuilder::buildWindowedPrologue() const {
  if (rng_ == nullptr) {
    throw std::invalid_argument(
        "LegitTransferBuilder.buildWindowedPrologue() requires a non-null rng");
  }

  const auto *accountRegistry = accounts();
  if (accountRegistry == nullptr || accountRegistry->records.empty()) {
    throw std::invalid_argument("LegitTransferBuilder.buildWindowedPrologue() "
                                "requires a populated account registry");
  }

  WindowedPrologue out;

  // RAM R2.4b-3: the windowed composition consumes only the screened
  // view during the prologue; the replay order is derived ONCE at spool
  // time (windowed_run.cpp). Skipping the second view halves the
  // resident stream and its merge transients. The monolithic build()
  // above keeps both views.
  out.streams.deferReplayView();

  auto plan = blueprints::buildLegitBlueprint(timeframe_, census_);
  plan.addCounterparties(*rng_, census_, counterparties_, hubSelection_)
      .addPersonas(*rng_, timeframe_, personas_);

  out.initialBook = openingBook_.build(plan);
  memlog::logPlain("openingBook");

  out.screen = ScreenBook{out.initialBook.get()};

  // Heap-pinned so the routine pass (and the session built later) can hold
  // a stable pointer across moves of this prologue.
  out.txf = std::make_unique<const transactions::Factory>(*rng_, router_);

  passes::addIncome(income_, plan, *out.txf, out.streams);
  memlog::log("income", out.streams);

  if (census_.ownership != nullptr && census_.accounts != nullptr) {
    auto routinePass = routines_;
    routinePass.accounts(passes::AccountAccess{
        .registry = census_.accounts,
        .ownership = census_.ownership,
    });
    routinePass.txf(*out.txf);
    passes::addRoutinesWithoutSpending(routinePass, plan, out.streams,
                                       out.screen);
    out.routinePass = routinePass;
  }

  memlog::log("routines:done", out.streams);

  out.plan = std::move(plan);
  return out;
}

} // namespace PhantomLedger::transfers::legit::ledger
