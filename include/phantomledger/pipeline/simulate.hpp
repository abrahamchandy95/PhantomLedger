#pragma once

#include "phantomledger/entropy/random/rng.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/pipeline/stages/infra.hpp"
#include "phantomledger/pipeline/stages/transfers.hpp"
#include "phantomledger/primitives/time/window.hpp"

#include <cstdint>

namespace PhantomLedger::pipeline {

struct SimulateInputs {
  ::PhantomLedger::time::Window window{};
  std::uint64_t seed = 0;

  ::PhantomLedger::pipeline::stages::entities::Inputs entities;
  ::PhantomLedger::pipeline::stages::infra::Inputs infraIn{};
  ::PhantomLedger::pipeline::stages::transfers::Inputs transfersIn{};
};

[[nodiscard]] SimulationResult simulate(::PhantomLedger::random::Rng &rng,
                                        const SimulateInputs &in);

} // namespace PhantomLedger::pipeline
