#pragma once

#include "phantomledger/entities/synth/pii/pools.hpp"
#include "phantomledger/pipeline/result.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::mule_ml {

struct Options {

  bool includeStandardExport = false;

  bool showTransactions = false;

  const ::PhantomLedger::entities::synth::pii::PoolSet *piiPools = nullptr;
};

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir,
               const Options &options = {});

} // namespace PhantomLedger::exporter::mule_ml
