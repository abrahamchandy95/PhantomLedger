#pragma once

#include "phantomledger/activity/spending/simulator/commerce_evolver.hpp"
#include "phantomledger/activity/spending/simulator/day_driver.hpp"
#include "phantomledger/activity/spending/simulator/day_source.hpp"
#include "phantomledger/activity/spending/simulator/population_dynamics.hpp"
#include "phantomledger/activity/spending/simulator/run_planner.hpp"
#include "phantomledger/activity/spending/simulator/spender_emission_driver.hpp"
#include "phantomledger/transfers/legit/routines/spending/behavior.hpp"

// Config-to-simulator wiring shared by the one-shot SpendingRoutine and
// the persistent SessionBundle: both engines must assemble the SAME
// planner and day driver from the same routines-level knobs, or their
// outputs drift apart and the session/simulator equivalence gate fails.

namespace PhantomLedger::transfers::legit::routines::spending {

namespace plSimulator = ::PhantomLedger::activity::spending::simulator;

[[nodiscard]] inline plSimulator::RunPlanner
plannerFrom(const RunPlanning &planning) {
  return plSimulator::RunPlanner{
      planning.load,
      planning.channels,
      planning.paymentRules,
  };
}

[[nodiscard]] inline plSimulator::DayDriver
dayDriverFrom(const DayPattern &day, const DynamicsProfile &dynamics,
              const EmissionProfile &emission) {
  return plSimulator::DayDriver{
      plSimulator::DaySource{day.variation, day.seasonal},
      plSimulator::CommerceEvolver{dynamics.commerce},
      plSimulator::PopulationDynamics{dynamics.population},
      plSimulator::SpenderEmissionDriver{
          plSimulator::SpenderEmissionDriver::Behavior{
              .baseExploreP = emission.baseExploreP,
              .exploration = emission.exploration,
              .liquidity = emission.liquidity,
              .rates = emission.rates,
          }},
  };
}

} // namespace PhantomLedger::transfers::legit::routines::spending
