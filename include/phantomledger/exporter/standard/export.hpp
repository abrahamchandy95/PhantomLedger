#pragma once

#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <filesystem>

namespace PhantomLedger::exporter::sinks {
struct PgMirror;
} // namespace PhantomLedger::exporter::sinks

namespace PhantomLedger::exporter::standard {

struct Options {
  bool showTransactions = false;

  bool emitEntityResolution = true;
  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  ::PhantomLedger::time::Window window{};

  ::PhantomLedger::synth::pii::Growth growth{};

  // When set, every table is ALSO written directly into PostgreSQL as
  // the same bytes the CSV file receives (CSV retirement arc). The
  // ledger CSV (transactions.csv, --show-transactions) is the one
  // exception and stays file-only: its stem would collide with the
  // streamed 'transactions' corpus table, which is canonical.
  const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;
};

// Complete standard export from a retained posted corpus (the default
// engine's path): entity tables plus the transaction-scale artifacts.
void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const std::filesystem::path &outDir,
               const Options &options = {});

// Entity-scale tables only (people, accounts, PII, infra, merchants,
// entity resolution). Ignores result.transfers entirely, so it accepts
// the windowed engine's world — pair it with StreamingTransfersExport
// (streaming.hpp), which produces the transaction-scale artifacts from
// the settled stream; together they reproduce exportAll byte-for-byte.
void exportEntities(const ::PhantomLedger::pipeline::SimulationResult &result,
                    const std::filesystem::path &outDir,
                    const Options &options = {});

} // namespace PhantomLedger::exporter::standard
