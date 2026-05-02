#include "phantomledger/pipeline/simulate.hpp"

namespace PhantomLedger::pipeline {

SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                          const SimulateInputs &in) {
  SimulationResult out;

  auto identity = in.entities.identity;
  if (identity.simStart == ::PhantomLedger::time::TimePoint{}) {
    identity.simStart = in.window.start;
  }

  auto infraIn = in.infraIn;
  if (infraIn.window.days == 0) {
    infraIn.window = in.window;
  }

  out.entities = ::PhantomLedger::pipeline::stages::entities::build(
      rng, in.window, identity, in.entities.population, in.entities.fraud,
      in.entities.personaMix, in.entities.merchants, in.entities.landlords,
      in.entities.counterparties, in.entities.cardIssuance, in.entities.seeds,
      in.entities.mortgage, in.entities.autoLoan, in.entities.studentLoan,
      in.entities.tax, in.entities.insurance);

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
