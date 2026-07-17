#pragma once

#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::sinks {
struct PgMirror;
} // namespace PhantomLedger::exporter::sinks

namespace PhantomLedger::exporter::mule_ml {

struct Options {
  bool showTransactions = false;

  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  // When set, every table is ALSO written directly into PostgreSQL as
  // the same bytes the CSV file receives (CSV retirement arc step 1);
  // the post-run csv_loader mirror pass then skips these tables.
  const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;
};

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir,
               const Options &options = {});

} // namespace PhantomLedger::exporter::mule_ml
