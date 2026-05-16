#pragma once

#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/pipeline/infra.hpp"
#include "phantomledger/pipeline/transfers.hpp"

namespace PhantomLedger::pipeline {

struct SimulationResult {
  People people;
  Holdings holdings;
  Counterparties counterparties;

  Infra infra;
  Transfers transfers;
};

} // namespace PhantomLedger::pipeline
