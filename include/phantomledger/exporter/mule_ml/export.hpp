#pragma once

#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/synth/pii/pools.hpp"

namespace PhantomLedger::exporter::sinks {
struct PgMirror;
} // namespace PhantomLedger::exporter::sinks

namespace PhantomLedger::exporter::common {
struct TableCapture;
} // namespace PhantomLedger::exporter::common

namespace PhantomLedger::exporter::mule_ml {

struct Options {
  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  // Every table is written directly into PostgreSQL as the bytes the
  // csv::Writer renders — the only production destination (CSV
  // retirement arc complete; PhantomLedger writes no files).
  const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

  // Test infrastructure: rendered bytes per table stem (serverless
  // exporter gates). Never set in production.
  ::PhantomLedger::exporter::common::TableCapture *capture = nullptr;
};

void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const Options &options = {});

} // namespace PhantomLedger::exporter::mule_ml
