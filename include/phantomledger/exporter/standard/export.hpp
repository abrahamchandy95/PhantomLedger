#pragma once

#include "phantomledger/pipeline/result.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::standard {

struct Options {

  bool showTransactions = false;
};

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir,
               const Options &options = {});

} // namespace PhantomLedger::exporter::standard
