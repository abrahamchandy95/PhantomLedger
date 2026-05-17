#pragma once

#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::mule_ml {

struct Options {
  bool showTransactions = false;

  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;
};

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir,
               const Options &options = {});

} // namespace PhantomLedger::exporter::mule_ml
