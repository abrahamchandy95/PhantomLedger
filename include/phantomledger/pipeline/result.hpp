#pragma once

#include "phantomledger/pipeline/state.hpp"

namespace PhantomLedger::pipeline {

struct SimulationResult {
  Entities entities;
  Infra infra;
  Transfers transfers;
};

} // namespace PhantomLedger::pipeline
