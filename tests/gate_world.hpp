#pragma once
//
// tests/gate_world.hpp
//
// GateWorld: the deterministic world-construction prefix shared by the
// windowed gate harness (window_leg_support.hpp runLeg), the
// session-vs-simulator bisect (test_session_vs_simulator.cpp), and the
// session window-invariance matrix (test_spending.cpp). The prefix used
// to be maintained as three keep-in-sync copies; it now lives here once.
//
// Construction mirrors SimulationPipeline::buildEntities() and
// LegitTransferBuilder::build() with default plans: entities, optional
// product synthesis, optional access infra (with pristine router
// snapshots), blueprint, opening book, optional income and base-routine
// passes, market/obligations preparation, and the card-lifecycle
// config. Two worlds built with the same WorldSpec consume the shared
// RNG identically; every flag exists because one gate must omit a
// layer.
//
// ROUTER SNAPSHOTS: the Router carries mutable sticky per-person
// device/IP state, so routing is order-dependent across generators.
// Products and family each route from their own pristine copy taken
// immediately after infra build (mirroring TransferStage::infra() and
// LegitTransferBuilder::build()), so neither can perturb — or be
// perturbed by — the sticky state the session shares with income and
// routines.
//
// FRAUD PROFILE: the production default plans ~6 rings per 10,000
// people and the count ROUNDS, so a 300-person world plans zero fraud
// participants and every fraud-boundary gate would be vacuous. The
// windowed gates therefore pass scaledFraudProfile(): the ring RATE is
// raised and its sigma zeroed (deterministic count, no extra RNG draw),
// which guarantees at least one ring at this population. The fraud
// BUDGET — targetTxnFraudP and the realized-corpus ratio — is
// untouched. This is leg-constant test configuration, not a model
// change.
//

#include "phantomledger/entities/institutional_accounts.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/products.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/people/fraud.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/synth/pii/samplers.hpp"
#include "phantomledger/taxonomies/enums.hpp"
#include "phantomledger/taxonomies/locale/types.hpp"
#include "phantomledger/transactions/clearing/balance_book.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"
#include "phantomledger/transactions/factory.hpp"
#include "phantomledger/transactions/record.hpp"
#include "phantomledger/transfers/channels/credit_cards/lifecycle.hpp"
#include "phantomledger/transfers/channels/government/disability.hpp"
#include "phantomledger/transfers/channels/government/retirement.hpp"
#include "phantomledger/transfers/legit/blueprints/plans.hpp"
#include "phantomledger/transfers/legit/ledger/limits.hpp"
#include "phantomledger/transfers/legit/ledger/passes.hpp"
#include "phantomledger/transfers/legit/ledger/screenbook.hpp"
#include "phantomledger/transfers/legit/ledger/streams.hpp"
#include "phantomledger/transfers/legit/routines/spending.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace pltest {

namespace pl = ::PhantomLedger;
namespace entityStage = pl::pipeline::stages::entities;
namespace infraStage = pl::pipeline::stages::infra;
namespace productStage = pl::pipeline::stages::products;
namespace legitBlueprints = pl::transfers::legit::blueprints;
namespace legitLedger = pl::transfers::legit::ledger;
namespace legitPasses = pl::transfers::legit::ledger::passes;
namespace routineSpending = pl::transfers::legit::routines::spending;
namespace cpKeys = pl::counterparties;

using Txn = pl::transactions::Transaction;

[[nodiscard]] inline pl::synth::pii::PoolSet buildPoolSet(std::uint64_t seed) {
  pl::synth::pii::PoolSet poolSet;
  pl::synth::pii::PoolSizes sizes;
  poolSet.byCountry[pl::taxonomies::enums::toIndex(pl::locale::Country::us)] =
      pl::synth::pii::buildLocalePool(pl::locale::Country::us, sizes,
                                      static_cast<std::uint32_t>(seed));
  return poolSet;
}

// See FRAUD PROFILE in the file comment. With participation ceiling
// 0.10 * population >= size.min, the first sampleRing() call cannot
// fail, so at least one ring (with mules and victims) exists at any
// seed.
[[nodiscard]] inline pl::synth::people::Fraud scaledFraudProfile() {
  pl::synth::people::Fraud profile{};
  profile.rings.perTenKMean = 200.0;
  profile.rings.perTenKSigma = 0.0;
  profile.solos.perTenK = 100.0;
  profile.limits.maxParticipationP = 0.10;
  return profile;
}

// Options for one GateWorld. Every option is leg-constant by
// construction within one gate; two worlds built from the same spec are
// byte-identical.
struct WorldSpec {
  std::uint64_t seed = 0;
  pl::time::Window window{};
  std::int32_t population = 300;

  // The profile entity synthesis plans with. The default is the
  // production default (the session matrix uses it); the windowed gates
  // pass scaledFraudProfile().
  pl::synth::people::Fraud fraudProfile{};

  bool withProducts = true;

  // Build the access infra and take the pristine router snapshots.
  // withInfraRouting additionally binds the shared router into the row
  // factory. The windowed gates keep withInfra on for every leg so
  // shared-RNG consumption is identical across one gate's option
  // combinations; only the routing bind varies.
  bool withInfra = true;
  bool withInfraRouting = true;

  bool withIncome = true;
  bool withBaseRoutines = false;
};

// Members are public and appear in construction order; the gates read
// them directly (this is a harness, not an API). The obligations
// snapshot spans the streams' screened view and cardCfg points into
// holdings and plan, so the object is pinned: no copies, no moves.
struct GateWorld {
  GateWorld(const pl::synth::pii::PoolSet &poolSet, const WorldSpec &spec);

  GateWorld(const GateWorld &) = delete;
  GateWorld &operator=(const GateWorld &) = delete;
  GateWorld(GateWorld &&) = delete;
  GateWorld &operator=(GateWorld &&) = delete;

  pl::random::Rng rng;

  // The profile the world was built with (the fraud injector reads the
  // same one, so gates never juggle two copies that must match).
  pl::synth::people::Fraud fraudProfile;

  pl::pipeline::People people;
  pl::pipeline::Holdings holdings;
  pl::pipeline::Counterparties cps;

  // Default-empty unless spec.withInfra.
  pl::pipeline::Infra infra;

  // See ROUTER SNAPSHOTS in the file comment.
  pl::infra::Router productRouter;
  pl::infra::Router familyRouter;

  legitBlueprints::LegitBlueprint plan;
  std::unique_ptr<pl::clearing::Ledger> initialBook;
  legitLedger::ScreenBook screen;
  legitLedger::TxnStreams streams;

  // Bound to rng at construction (hence optional); routes device/IP
  // through infra.router only when spec.withInfraRouting.
  std::optional<pl::transactions::Factory> txf;

  pl::activity::spending::market::Market market;
  pl::activity::spending::obligations::Snapshot obligations;

  // screen.fresh(); never null after construction.
  pl::clearing::Ledger *screenBook = nullptr;

  // Mirrors passes.cpp's buildCardLifecycleConfig().
  routineSpending::SpendingRoutine::CardLifecycleConfig cardCfg;
};

inline GateWorld::GateWorld(const pl::synth::pii::PoolSet &poolSet,
                            const WorldSpec &spec)
    : rng(pl::random::Rng::fromSeed(spec.seed)),
      fraudProfile(spec.fraudProfile) {
  // Routing draws go through the infra router; there is nothing to bind
  // when the infra was never built.
  PL_CHECK(!spec.withInfraRouting || spec.withInfra);

  const pl::synth::pii::IdentityContext identity{
      .pools = &poolSet,
      .simStart = spec.window.start,
      .localeMix = pl::synth::pii::LocaleMix::usOnly(),
  };

  // Mirrors SimulationPipeline::buildEntities() with default plans.
  people.roster = entityStage::buildPeople(rng, spec.population, fraudProfile);
  holdings.accounts =
      entityStage::buildAccounts(rng, people.roster, spec.population);
  people.personas = entityStage::buildPersonas(rng, people.roster);
  people.pii = entityStage::buildPii(rng, people.personas, identity,
                                     people.roster.topology,
                                     pl::synth::pii::Sharing{});
  cps.merchants = entityStage::buildMerchants(rng, spec.population);
  cps.landlords = entityStage::buildLandlords(rng, spec.population);
  cps.counterparties = entityStage::buildCounterparties(rng, spec.population);
  holdings.creditCards =
      entityStage::issueCreditCards(people.personas, people.roster, spec.seed);
  entityStage::finalizeAccountRegistry(holdings, cps, people);
  entityStage::synthesizeBusinessOwners(holdings, people, rng);

  if (spec.withProducts) {
    // Products use their own content-keyed seed; window size cannot
    // reach it.
    productStage::ObligationSynthesis{}.synthesize(people, holdings,
                                                   spec.window);
  }

  if (spec.withInfra) {
    infra = infraStage::AccessInfraStage{}.build(rng, people, holdings,
                                                 spec.window);

    // Pristine snapshots, taken before any income or routine emission
    // touches the shared router's sticky state.
    productRouter = infra.router;
    familyRouter = infra.router;
  }

  // Mirrors LegitTransferBuilder::build()'s blueprint construction.
  const legitBlueprints::LegitTimeframe timeframe{
      .window = spec.window,
      .seed = spec.seed,
  };
  const legitBlueprints::AccountCensus census{
      .accounts = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };

  plan = legitBlueprints::buildLegitBlueprint(timeframe, census);
  plan.addCounterparties(
          rng, census,
          legitBlueprints::CounterpartyPools{
              .directory = &cps.counterparties,
              .landlords = &cps.landlords.roster,
          },
          legitBlueprints::HubSelectionRules{
              .populationCount = people.roster.roster.count,
              .fraction = 0.01,
          })
      .addPersonas(rng, timeframe,
                   legitBlueprints::PersonaCatalog{.pack = &people.personas});

  const legitLedger::OpeningBook openingBook{
      rng,
      legitLedger::OpeningBook::Accounts{
          .registry = &holdings.accounts.registry,
          .lookup = &holdings.accounts.lookup,
          .ownership = &holdings.accounts.ownership,
      },
      legitLedger::OpeningBook::Protections{
          .balanceRules = &pl::clearing::kDefaultBalanceRules,
          .portfolios = &holdings.portfolios,
          .creditCards = &holdings.creditCards,
      },
  };
  initialBook = openingBook.build(plan);
  PL_CHECK(initialBook != nullptr);

  screen = legitLedger::ScreenBook{initialBook.get()};

  txf.emplace(rng, spec.withInfraRouting ? &infra.router : nullptr);

  const legitPasses::AccountAccess accountAccess{
      .registry = &holdings.accounts.registry,
      .ownership = &holdings.accounts.ownership,
  };

  if (spec.withIncome) {
    // Income (salary, government benefits, revenue) mirrors
    // passes::addIncome. Its payday inbounds are what future-inbound
    // cure discovery finds.
    const pl::transfers::government::RetirementTerms retirement{};
    const pl::transfers::government::DisabilityTerms disability{};

    const legitPasses::IncomePass incomePass{
        &rng,
        accountAccess,
        legitPasses::SalarySetup{
            .revenueCounterparties = &cps.counterparties,
            .rules = {},
        },
        legitPasses::GovernmentSetup{
            .counterparties =
                legitPasses::GovernmentCounterparties{
                    .ssa = cpKeys::key(cpKeys::Government::ssa),
                    .disability = cpKeys::key(cpKeys::Government::disability),
                },
            .retirement = &retirement,
            .disability = &disability,
        },
    };
    legitPasses::addIncome(incomePass, plan, *txf, streams);
  }

  if (spec.withBaseRoutines) {
    // The exact monolithic base-stream generation, minus spending.
    auto routinePass = legitPasses::RoutinePass{
        &rng,
        accountAccess,
        legitPasses::RoutineResources{
            .accountsLookup = &holdings.accounts.lookup,
            .merchants = &cps.merchants,
            .portfolios = &holdings.portfolios,
            .creditCards = &holdings.creditCards,
            .cardLifecycle =
                &pl::transfers::credit_cards::kDefaultLifecycleRules,
        },
    };
    routinePass.txf(*txf);
    legitPasses::addRoutinesWithoutSpending(routinePass, plan, streams, screen);
  }

  // Market and obligations read the accumulated stream exactly as
  // addSpending reads streams.screened() — family rows are deliberately
  // NOT in it (the monolithic path generates family after spending).
  // The screened view must stay untouched for the session's lifetime:
  // the obligations snapshot holds a span into it.
  const routineSpending::SpendingRoutine routine;
  const routineSpending::SpendingRoutine::CensusSource censusSource{
      .blueprint = plan,
      .accounts =
          routineSpending::SpendingRoutine::AccountSource{
              .lookup = holdings.accounts.lookup,
              .registry = holdings.accounts.registry,
          },
  };

  const std::span<const Txn> baseTxns(streams.screened());

  market = routine.prepareMarket(
      censusSource,
      routineSpending::SpendingRoutine::PayeeDirectory{
          .merchants = &cps.merchants,
          .creditCards = &holdings.creditCards,
      },
      baseTxns);

  obligations = routineSpending::SpendingRoutine::prepareObligations(
      censusSource,
      routineSpending::SpendingRoutine::ObligationSource{
          .portfolios = &holdings.portfolios,
      },
      baseTxns,
      /*baseTxnsSorted=*/true);

  screenBook = screen.fresh();
  PL_CHECK(screenBook != nullptr);

  // Mirrors passes.cpp's buildCardLifecycleConfig().
  cardCfg.cards = &holdings.creditCards;
  cardCfg.rules = &pl::transfers::credit_cards::kDefaultLifecycleRules;
  cardCfg.issuerAccount = plan.counterparties().issuerAcct;
  cardCfg.window = spec.window;
  cardCfg.seed = spec.seed;
  cardCfg.primaryAccounts.reserve(plan.primaryAcctRecordIx().size());
  for (const auto &kv : plan.primaryAcctRecordIx()) {
    const auto &record = plan.allAccounts()->records[kv.second];
    cardCfg.primaryAccounts.emplace(kv.first, record.id);
  }
}

} // namespace pltest
