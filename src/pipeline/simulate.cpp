#include "phantomledger/pipeline/simulate.hpp"

namespace PhantomLedger::pipeline {

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          const SimulateInputs &in) {
  SimulationResult out;

  auto entitiesIn = in.entitiesIn;
  if (entitiesIn.simStart == ::PhantomLedger::time::TimePoint{}) {
    entitiesIn.simStart = in.window.start;
  }
  if (entitiesIn.window.days == 0) {
    entitiesIn.window = in.window;
  }

  auto infraIn = in.infraIn;
  if (infraIn.window.days == 0) {
    infraIn.window = in.window;
  }

  out.entities =
      ::PhantomLedger::pipeline::stages::entities::build(rng, entitiesIn);

  out.infra = ::PhantomLedger::pipeline::stages::infra::build(rng, out.entities,
                                                              infraIn);

  auto transfersIn = in.transfersIn;
  if (transfersIn.window.days == 0) {
    transfersIn.window = in.window;
  }
  if (transfersIn.seed == 0) {
    transfersIn.seed = in.seed;
  }
  out.transfers = ::PhantomLedger::pipeline::stages::transfers::build(
      rng, out.entities, out.infra, transfersIn);

  return out;
}

} // namespace PhantomLedger::pipeline
