#pragma once

#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::standard {

struct Options {
  bool showTransactions = false;

  bool emitEntityResolution = true;
  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  ::PhantomLedger::time::Window window{};

  ::PhantomLedger::synth::pii::Growth growth{};
};

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir,
               const Options &options = {});

} // namespace PhantomLedger::exporter::standard
