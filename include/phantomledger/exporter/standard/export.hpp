#pragma once
/*
  Standard Export Entry Point
  The default use case's tables. `exportAll` needs a retained posted corpus;
  `exportEntities` writes only the entity-scale half, so the windowed engine
  can pair it with StreamingTransfersExport and reproduce exportAll's bytes.
 */

#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"

namespace PhantomLedger::exporter::sinks {
struct PgMirror;
} // namespace PhantomLedger::exporter::sinks

namespace PhantomLedger::exporter::common {
struct TableCapture;
} // namespace PhantomLedger::exporter::common

namespace PhantomLedger::exporter::standard {

struct Options {
  bool emitEntityResolution = true;
  const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

  /* The membership view [joinTs, closeTs) derives from the result's personas
   * pack via join_cohort::membershipOf(pack, window) — the world models
   * membership, the exporter only reads it. */
  ::PhantomLedger::time::Window window{};

  /* Every table is written directly into PostgreSQL as the bytes the
   * csv::Writer renders — the only production destination (PhantomLedger
   * writes no files). */
  const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

  /* Test infrastructure: rendered bytes per table stem (serverless exporter
   * gates). Never set in production. */
  ::PhantomLedger::exporter::common::TableCapture *capture = nullptr;
};

/* Complete standard export from a retained posted corpus (the default
 * engine's path): entity tables plus the transaction-scale artifacts. */
void exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
               const Options &options = {});

/* Entity-scale tables only (people, accounts, PII, infra, merchants, entity
 * resolution). Ignores result.transfers entirely, so it accepts the windowed
 * engine's world — pair it with StreamingTransfersExport (streaming.hpp),
 * which produces the transaction-scale artifacts from the settled stream;
 * together they reproduce exportAll byte-for-byte. */
void exportEntities(const ::PhantomLedger::pipeline::SimulationResult &result,
                    const Options &options = {});

} // namespace PhantomLedger::exporter::standard
