#include "phantomledger/transfers/legit/routines/spending_session.hpp"

#include "phantomledger/activity/spending/simulator/commerce_evolver.hpp"
#include "phantomledger/activity/spending/simulator/day_driver.hpp"
#include "phantomledger/activity/spending/simulator/day_source.hpp"
#include "phantomledger/activity/spending/simulator/population_dynamics.hpp"
#include "phantomledger/activity/spending/simulator/run_planner.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/activity/spending/simulator/thread_runner.hpp"

#include <stdexcept>
#include <utility>

namespace PhantomLedger::transfers::legit::routines::spending {

namespace {

[[nodiscard]] plSimulator::RunPlanner plannerFrom(const RunPlanning &planning) {
  return plSimulator::RunPlanner{
      planning.load,
      planning.channels,
      planning.paymentRules,
  };
}

[[nodiscard]] plSimulator::DayDriver
dayDriverFrom(const DayPattern &day, const DynamicsProfile &dynamics,
              const EmissionProfile &emission) {
  return plSimulator::DayDriver{
      plSimulator::DaySource{
          day.variation,
          day.seasonal,
      },
      plSimulator::CommerceEvolver{
          dynamics.commerce,
      },
      plSimulator::PopulationDynamics{
          dynamics.population,
      },
      plSimulator::SpenderEmissionDriver{
          plSimulator::SpenderEmissionDriver::Behavior{
              .baseExploreP = emission.baseExploreP,
              .exploration = emission.exploration,
              .liquidity = emission.liquidity,
              .rates = emission.rates,
          },
      },
  };
}

} // namespace

std::unique_ptr<SessionBundle> SessionBundle::make(
    std::uint64_t seed, random::Rng &rng, const transactions::Factory &txf,
    plMarketNs::Market &market, const plObligationsNs::Snapshot &obligations,
    clearing::Ledger *screenBook, SessionInputs inputs) {
  if (inputs.threadCount.has_value() && *inputs.threadCount == 0) {
    throw std::invalid_argument("SessionBundle threadCount must be positive");
  }

  // The constructor is private, so std::make_unique cannot invoke it.
  auto bundle = std::unique_ptr<SessionBundle>(new SessionBundle(seed));

  bundle->inputs_ = std::move(inputs);

  const auto resolvedThreadCount =
      bundle->inputs_.threadCount.value_or(plSimulator::resolveThreadCount());

  const plSimulator::SpenderEmissionDriver::Threads threads{
      .rngFactory = &bundle->rngFactory_,
      .count = resolvedThreadCount,
  };

  auto &cardConfig = bundle->inputs_.cardLifecycle;

  if (cardConfig.active()) {
    plCreditNs::DriverInputs driverInputs{
        .cards = cardConfig.cards,

        // The map belongs to bundle->inputs_, so its address remains valid
        // for the complete CardCycleDriver lifetime.
        .primaryAccounts = &cardConfig.primaryAccounts,

        .issuerAccount = cardConfig.issuerAccount,

        .window = cardConfig.window,
    };

    const auto cardSeed = cardConfig.seed != 0 ? cardConfig.seed : seed;

    bundle->cardDriver_ = std::make_unique<plCreditNs::CardCycleDriver>(
        *cardConfig.rules, txf, random::RngFactory{cardSeed}, driverInputs,
        screenBook);
  }

  bundle->session_ =
      std::make_unique<plSimulator::Session>(market, rng, txf, obligations);

  bundle->session_->ledger(screenBook)
      .planner(plannerFrom(bundle->inputs_.planning))
      .dayDriver(dayDriverFrom(bundle->inputs_.day, bundle->inputs_.dynamics,
                               bundle->inputs_.emission))
      .emissionThreads(threads)
      .cardCycleBilling(
          bundle->cardDriver_ != nullptr ? bundle->cardDriver_.get() : nullptr);

  return bundle;
}

} // namespace PhantomLedger::transfers::legit::routines::spending
