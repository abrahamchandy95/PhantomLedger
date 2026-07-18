#include "phantomledger/app/cli.hpp"
#include "phantomledger/app/options.hpp"
#include "phantomledger/app/progress.hpp"
#include "phantomledger/app/setup.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/streaming.hpp"
#include "phantomledger/exporter/aml_txn_edges/export.hpp"
#include "phantomledger/exporter/aml_txn_edges/readback.hpp"
#include "phantomledger/exporter/aml_txn_edges/streaming.hpp"
#include "phantomledger/exporter/mule_ml/export.hpp"
#include "phantomledger/exporter/mule_ml/streaming.hpp"
#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/exporter/sinks/postgres.hpp"
#include "phantomledger/exporter/sinks/run_ledger.hpp"
#include "phantomledger/exporter/sinks/table_mirror.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/exporter/standard/streaming.hpp"
#include "phantomledger/pipeline/chunk/flush.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/calendar.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/synth/pii/samplers.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/resource.h>

namespace {

namespace pl = ::PhantomLedger;

double peakRssMB() {
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
  return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(ru.ru_maxrss) / 1024.0;
#endif
}

class PhaseMonitor {
public:
  PhaseMonitor() : start_(Clock::now()), last_(start_) {}

  void mark(std::string_view label) {
    const auto now = Clock::now();
    const auto deltaMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_)
            .count();
    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_)
            .count();
    std::fprintf(
        stderr, "[phase] %-24s  +%9.1fs  (total %9.1fs)  peakRSS=%9.1f MB\n",
        std::string{label}.c_str(), static_cast<double>(deltaMs) / 1000.0,
        static_cast<double>(totalMs) / 1000.0, peakRssMB());
    last_ = now;
  }

private:
  using Clock = std::chrono::steady_clock;
  Clock::time_point start_;
  Clock::time_point last_;
};

// -------------------------------------------------------- backend policy
//
// PostgreSQL is a requirement of every run, not an option: the corpus
// streams into the 'transactions' table during settlement, every
// exporter writes its tables directly (no files are written), and the
// derived analytics read the corpus back. Probe BEFORE any generation
// and fail fast with an actionable error — the error message IS the
// prompt (CI-safe, never interactive). PL_FILE_ONLY=1 is the harness
// escape that keeps the golden and the serverless equivalence gates
// independent of any server; under it a run persists nothing and
// yields only the stream digest.

struct BackendPolicy {
  std::string conninfo;
  bool fileOnly = false; // PL_FILE_ONLY: no server (serverless gates)
};

[[nodiscard]] BackendPolicy resolveBackend(const pl::app::RunOptions &opts) {
  BackendPolicy policy;
  policy.conninfo = opts.pgConninfo;
  if (const char *env = std::getenv("PL_PG")) {
    policy.conninfo = env;
  }
  if (const char *env = std::getenv("PL_FILE_ONLY")) {
    const std::string_view value{env};
    policy.fileOnly = !value.empty() && value != "0";
  }

  if (policy.fileOnly) {
    std::fprintf(stderr, "note: PL_FILE_ONLY set — skipping PostgreSQL "
                         "(harness escape); nothing is persisted\n");
    return policy;
  }

  try {
    pl::postgres::Connection probe{policy.conninfo};
  } catch (const std::exception &err) {
    std::fprintf(
        stderr,
        "fatal: PostgreSQL is required and no server answered via '%s'.\n"
        "\n"
        "PhantomLedger streams every corpus into PostgreSQL (table\n"
        "'transactions') during settlement and reads it back for the\n"
        "derived analytics, so a reachable server is part of every run.\n"
        "PostgreSQL is free:\n"
        "\n"
        "  point at any server   PL_PG='host=... port=... dbname=... "
        "user=...'\n"
        "  or install locally    macOS:  brew install postgresql@17\n"
        "                        Debian: sudo apt install postgresql\n"
        "                        then:   createdb phantomledger\n"
        "\n"
        "Reruns with the same seed and config are idempotent: data tables\n"
        "are fully rewritten with byte-identical content.\n"
        "\n"
        "Server said: %s",
        policy.conninfo.c_str(), err.what());
    std::exit(1);
  }
  return policy;
}

[[nodiscard]] const char *directSchemaNote(pl::app::UseCase uc) noexcept {
  switch (uc) {
  case pl::app::UseCase::standard:
    return "public schema";
  case pl::app::UseCase::muleMl:
    return "schema mule_ml";
  case pl::app::UseCase::aml:
    return "schema aml";
  case pl::app::UseCase::amlTxnEdges:
    return "schema aml_txn_edges";
  }
  return "";
}

// ------------------------------------------------------------- summaries

void printGenericSummary(const pl::pipeline::SimulationResult &result) {
  const auto &postedTxns = result.transfers.ledger.posted.txns;
  const auto totalTxns = postedTxns.size();

  std::size_t illicit = 0;
  for (const auto &tx : postedTxns) {
    if (tx.fraud.flag != 0) {
      ++illicit;
    }
  }

  const double ratio =
      (totalTxns == 0) ? 0.0 : static_cast<double>(illicit) / totalTxns;

  std::printf("People: %u  Accounts: %zu\n",
              static_cast<unsigned>(result.people.roster.roster.count),
              result.holdings.accounts.registry.records.size());
  std::printf("Transactions: %zu  Illicit: %zu (%.4f%%)\n", totalTxns, illicit,
              ratio * 100.0);
}

void printWindowedSummary(
    const pl::pipeline::SimulationResult &world,
    const pl::pipeline::stages::transfers::WindowedRunResult &transfers,
    std::uint64_t streamRows) {
  const auto &summary = transfers.summary;

  const double ratio =
      (streamRows == 0)
          ? 0.0
          : static_cast<double>(summary.phaseB.fraudRows) / streamRows;

  std::printf("People: %u  Accounts: %zu\n",
              static_cast<unsigned>(world.people.roster.roster.count),
              world.holdings.accounts.registry.records.size());
  std::printf("Transactions: %llu  Fraud rows: %llu (%.4f%%)  candidates "
              "L=%llu  card events=%llu\n",
              static_cast<unsigned long long>(streamRows),
              static_cast<unsigned long long>(summary.phaseB.fraudRows),
              ratio * 100.0,
              static_cast<unsigned long long>(summary.phaseA.candidateRows),
              static_cast<unsigned long long>(summary.phaseA.cardEvents));
  std::printf("Candidate spool: %llu rows, %.1f MiB on disk\n",
              static_cast<unsigned long long>(transfers.spoolRows),
              static_cast<double>(transfers.spoolBytes) / (1024.0 * 1024.0));
  std::printf("Posted book hash: 0x%llx\n",
              static_cast<unsigned long long>(
                  transfers.postedBookHash));
}

void printAmlSummary(const pl::exporter::aml::Summary &summary) {

  const double ratio = (summary.totalTxnCount == 0)
                           ? 0.0
                           : static_cast<double>(summary.illicitTxnCount) /
                                 summary.totalTxnCount;

  std::printf("AML export complete\n");
  std::printf("  Customers:       %zu\n", summary.customerCount);
  std::printf("  Accounts:        %zu\n", summary.internalAccountCount);
  std::printf("  Counterparties:  %zu\n", summary.counterpartyCount);
  std::printf("  Transactions:    %zu  (illicit: %zu, %.4f%%)\n",
              summary.totalTxnCount, summary.illicitTxnCount, ratio * 100.0);
  std::printf("  Fraud rings:     %zu\n", summary.fraudRingCount);
  std::printf("  Solo fraudsters: %zu\n", summary.soloFraudCount);
  std::printf("  SARs filed:      %zu\n", summary.sarsFiledCount);
}

void printAmlTxnEdgesSummary(
    const pl::exporter::aml_txn_edges::Summary &summary) {

  const double ratio = (summary.totalTxnCount == 0)
                           ? 0.0
                           : static_cast<double>(summary.illicitTxnCount) /
                                 summary.totalTxnCount;

  std::printf("AML (txn-edges) export complete\n");
  std::printf("  Customers:       %zu\n", summary.customerCount);
  std::printf("  Accounts:        %zu\n", summary.internalAccountCount);
  std::printf("  Counterparties:  %zu\n", summary.counterpartyCount);
  std::printf("  Transactions:    %zu  (illicit: %zu, %.4f%%)\n",
              summary.totalTxnCount, summary.illicitTxnCount, ratio * 100.0);
  std::printf("  Fraud rings:     %zu\n", summary.fraudRingCount);
  std::printf("  Solo fraudsters: %zu\n", summary.soloFraudCount);
  std::printf("  SARs filed:      %zu\n", summary.sarsFiledCount);
  std::printf("  Alerts:          %zu  (CTRs: %zu)\n", summary.alertCount,
              summary.ctrCount);
  std::printf("  Cases:           %zu  (businesses: %zu)\n", summary.caseCount,
              summary.businessCount);
  std::printf("  Flow-agg edges:  %zu  (link-comm: %zu)\n",
              summary.flowAggEdgeCount, summary.linkCommEdgeCount);
}

// ------------------------------------------------------- windowed engine
//
// Bounded-memory path and the production DEFAULT for every use case
// (app::resolveEngine): the world is built first, then the transfer
// fold streams settled transactions straight into Golden + PostgreSQL
// + the use case's streaming exporter, whose tables write directly
// into PostgreSQL (one csv::Writer rendering — the COPY payload). The
// posted corpus is never materialized; the candidate corpus lives in
// the on-disk binary spool. aml-txn-edges additionally REQUIRES the
// server — its derived analytics read the streamed corpus back
// (readback::buildBundle) — which the backend policy guarantees; under
// the PL_FILE_ONLY harness escape it fails with its own clear error.
// Output is byte-identical to the monolithic reference engine.
//
// Checkpoint/resume (RunLedger + ResumableSpanSink, test_resume):
// determinism is the checkpoint — regenerate, verify committed spans
// by digest + lockstep read-back, skip their COPYs, resume writes.

int runWindowedStream(
    const pl::app::RunOptions &opts, pl::time::Window window,
    const pl::synth::pii::PoolSet &pools,
    const pl::pipeline::stages::entities::EntitySynthesis &entityConfig,
    const std::string &pgConninfo, bool pgUp) {
  namespace pg = pl::app::progress;

  auto rng = pl::random::Rng::fromSeed(opts.seed);
  pl::pipeline::SimulationPipeline pipeline{rng, window, entityConfig,
                                            opts.seed};

  PhaseMonitor mon;

  const bool streamStandard = opts.usecase == pl::app::UseCase::standard;
  const bool streamMuleMl = opts.usecase == pl::app::UseCase::muleMl;
  const bool streamAml = opts.usecase == pl::app::UseCase::aml;
  const bool streamAmlTxn = opts.usecase == pl::app::UseCase::amlTxnEdges;

  if (streamAmlTxn && !pgUp) {
    std::fprintf(
        stderr,
        "fatal: --usecase aml-txn-edges requires PostgreSQL: the derived "
        "analytics (alerts, cases, 30/90-day aggregates) read the "
        "streamed corpus back from the transactions table. PL_FILE_ONLY "
        "cannot run it windowed — unset PL_FILE_ONLY, or force the "
        "reference engine with PL_ENGINE=monolithic.\n");
    return 1;
  }

  // World first: the streaming exporters bind to the account registry
  // (and membership, for standard) before the fold starts.
  pl::pipeline::SimulationResult world;
  {
    pg::Stage genStage("Generating (windowed: world)", 3);
    const auto onPhase = [&](std::string_view phase) {
      mon.mark(phase);
      genStage.tick();
      genStage.setLabel("Generating (" + std::string{phase} + " done)");
    };
    world = pipeline.buildWorld(onPhase);
  } // genStage destructor prints trailing newline here

  // With the server up, EVERY use case's tables are written directly
  // into PostgreSQL as the bytes the csv::Writer renders (standard
  // lands unprefixed in the public schema; the aml exporters derive
  // their vertices_/edges_ prefixes internally).
  const pl::exporter::sinks::PgMirror stdMirror{.conninfo = pgConninfo,
                                                .schema = "",
                                                .tablePrefix = ""};
  const pl::exporter::sinks::PgMirror mlMirror{.conninfo = pgConninfo,
                                               .schema = "mule_ml",
                                               .tablePrefix = "ml_ready_"};
  const pl::exporter::sinks::PgMirror amlMirror{.conninfo = pgConninfo,
                                                .schema = "aml",
                                                .tablePrefix = "aml_"};
  const pl::exporter::sinks::PgMirror amlTxnMirror{
      .conninfo = pgConninfo,
      .schema = "aml_txn_edges",
      .tablePrefix = "aml_txn_edges_"};

  std::optional<pl::exporter::standard::StreamingTransfersExport> stdStream;
  if (streamStandard) {
    const auto population =
        static_cast<std::size_t>(world.people.roster.roster.count);
    stdStream.emplace(pl::exporter::standard::StreamingTransfersExport::Config{
        .registry = &world.holdings.accounts.registry,
        .lookup = &world.holdings.accounts.lookup,
        .membership = pl::synth::pii::Membership(population, window,
                                                 pl::synth::pii::Growth{}),
        .window = window,
        .pgMirror = pgUp ? &stdMirror : nullptr,
    });
  }

  std::optional<pl::exporter::mule_ml::StreamingMuleMlExport> mlStream;
  if (streamMuleMl) {
    mlStream.emplace(pl::exporter::mule_ml::StreamingMuleMlExport::Config{
        .registry = &world.holdings.accounts.registry,
        .roster = &world.people.roster.roster,
        .pii = &world.people.pii,
        .devices = &world.infra.devices,
        .ips = &world.infra.ips,
        .piiPools = &pools,
        .pgMirror = pgUp ? &mlMirror : nullptr,
    });
  }

  std::optional<pl::exporter::aml::StreamingAmlExport> amlStream;
  if (streamAml) {
    amlStream.emplace(pl::exporter::aml::StreamingAmlExport::Config{
        .people = &world.people,
        .holdings = &world.holdings,
        .piiPools = &pools,
        .pgMirror = pgUp ? &amlMirror : nullptr,
    });
  }

  std::optional<pl::exporter::aml_txn_edges::StreamingAmlTxnEdgesExport>
      amlTxnStream;
  if (streamAmlTxn) {
    amlTxnStream.emplace(
        pl::exporter::aml_txn_edges::StreamingAmlTxnEdgesExport::Config{
            .people = &world.people,
            .holdings = &world.holdings,
            .piiPools = &pools,
            .pgMirror = pgUp ? &amlTxnMirror : nullptr,
        });
  }

  pl::exporter::sinks::Golden golden;
  pl::pipeline::stages::transfers::WindowedRunResult transfers;

  const auto runFold = [&](auto &sink) {
    pg::Stage foldStage("Generating (windowed: transfers)", 1);
    const auto onPhase = [&](std::string_view phase) {
      mon.mark(phase);
      foldStage.tick();
    };
    return pipeline.runWindowedTransfers(world, sink, {}, onPhase);
  }; // foldStage destructor prints trailing newline per call

  std::uint64_t pgRows = 0;
  unsigned pgSpans = 0;
  unsigned pgSkipped = 0;

  if (pgUp) {
    pl::postgres::Connection conn{pgConninfo};
    pl::exporter::sinks::RunLedger ledger{conn};
    ledger.ensureTables();

    const auto configHash = pl::exporter::sinks::RunLedger::configHash(
        "windowed", opts.seed, opts.population, opts.days, opts.startDate);

    auto plan = ledger.findResumable(configHash);
    if (plan.has_value() && !ledger.prepareResume(*plan, "transactions")) {
      std::fprintf(stderr,
                   "warning: interrupted run #%lld is not resumable (the "
                   "transactions table changed since it stopped); marking "
                   "it failed and rewriting in full\n",
                   plan->manifestId);
      ledger.markFailed(plan->manifestId);
      plan.reset();
    }

    long long manifestId = -1;
    if (plan.has_value()) {
      manifestId = plan->manifestId;
      std::printf("Resuming run #%lld: %zu spans (%llu rows) already "
                  "durable — verifying by digest, skipping their COPYs\n",
                  plan->manifestId, plan->spans.size(),
                  static_cast<unsigned long long>(plan->rows));
    } else {
      // A fresh run rewrites the shared table: no older crash record
      // may outlive its rows.
      ledger.supersedeRunning();
      manifestId = ledger.beginRun(configHash, opts.seed, opts.population,
                                   opts.days, opts.startDate);
    }

    pl::exporter::sinks::Postgres pgSink(
        {.conninfo = pgConninfo,
         .createTable = !plan.has_value(),
         .truncateFirst = !plan.has_value(),
         .startRowSeq = plan.has_value() ? plan->rows : 0});
    pl::exporter::sinks::GatedSink<pl::exporter::sinks::Postgres> gatedPg{
        .inner = &pgSink};
    pl::pipeline::chunk::Tee teePg{golden, gatedPg};

    const pl::exporter::sinks::ResumePlan *planPtr =
        plan.has_value() ? &*plan : nullptr;

    if (stdStream.has_value()) {
      pl::pipeline::chunk::Tee teeAll{teePg, *stdStream};
      pl::exporter::sinks::ResumableSpanSink<decltype(teeAll)> sink{
          {.inner = &teeAll,
           .copyGate = &gatedPg.open,
           .ledger = &ledger,
           .manifestId = manifestId,
           .plan = planPtr,
           .conninfo = pgConninfo}};
      transfers = runFold(sink);
      pgSpans = sink.spansWritten();
      pgSkipped = sink.spansSkipped();
    } else if (mlStream.has_value()) {
      pl::pipeline::chunk::Tee teeAll{teePg, *mlStream};
      pl::exporter::sinks::ResumableSpanSink<decltype(teeAll)> sink{
          {.inner = &teeAll,
           .copyGate = &gatedPg.open,
           .ledger = &ledger,
           .manifestId = manifestId,
           .plan = planPtr,
           .conninfo = pgConninfo}};
      transfers = runFold(sink);
      pgSpans = sink.spansWritten();
      pgSkipped = sink.spansSkipped();
    } else if (amlStream.has_value()) {
      pl::pipeline::chunk::Tee teeAll{teePg, *amlStream};
      pl::exporter::sinks::ResumableSpanSink<decltype(teeAll)> sink{
          {.inner = &teeAll,
           .copyGate = &gatedPg.open,
           .ledger = &ledger,
           .manifestId = manifestId,
           .plan = planPtr,
           .conninfo = pgConninfo}};
      transfers = runFold(sink);
      pgSpans = sink.spansWritten();
      pgSkipped = sink.spansSkipped();
    } else if (amlTxnStream.has_value()) {
      pl::pipeline::chunk::Tee teeAll{teePg, *amlTxnStream};
      pl::exporter::sinks::ResumableSpanSink<decltype(teeAll)> sink{
          {.inner = &teeAll,
           .copyGate = &gatedPg.open,
           .ledger = &ledger,
           .manifestId = manifestId,
           .plan = planPtr,
           .conninfo = pgConninfo}};
      transfers = runFold(sink);
      pgSpans = sink.spansWritten();
      pgSkipped = sink.spansSkipped();
    } else {
      pl::exporter::sinks::ResumableSpanSink<decltype(teePg)> sink{
          {.inner = &teePg,
           .copyGate = &gatedPg.open,
           .ledger = &ledger,
           .manifestId = manifestId,
           .plan = planPtr,
           .conninfo = pgConninfo}};
      transfers = runFold(sink);
      pgSpans = sink.spansWritten();
      pgSkipped = sink.spansSkipped();
    }

    pgRows = pgSink.rowsWritten();
    if (pgSkipped > 0) {
      std::printf("PostgreSQL: %llu rows total -> table 'transactions' "
                  "(%u spans copied this run, %u verified and skipped)\n",
                  static_cast<unsigned long long>(pgRows), pgSpans, pgSkipped);
    } else {
      std::printf("PostgreSQL: %llu rows -> table 'transactions' (%u spans, "
                  "streamed during settlement)\n",
                  static_cast<unsigned long long>(pgRows), pgSpans);
    }
    ledger.finishRun(manifestId, golden.rowsWritten(), golden.digest());
  } else if (stdStream.has_value()) {
    pl::pipeline::chunk::Tee tee{golden, *stdStream};
    transfers = runFold(tee);
  } else if (mlStream.has_value()) {
    pl::pipeline::chunk::Tee tee{golden, *mlStream};
    transfers = runFold(tee);
  } else if (amlStream.has_value()) {
    pl::pipeline::chunk::Tee tee{golden, *amlStream};
    transfers = runFold(tee);
  } else {
    // aml-txn-edges cannot land here: it is guarded to require pgUp.
    transfers = runFold(golden);
  }

  std::printf("Stream digest: %s  rows: %llu\n", golden.digest().c_str(),
              static_cast<unsigned long long>(golden.rowsWritten()));
  mon.mark("stream flush");

  if (streamStandard) {
    pg::status("Exporting entity tables...");
    pl::exporter::standard::Options exportOpts;
    exportOpts.piiPools = &pools;
    exportOpts.window = window;
    exportOpts.pgMirror = pgUp ? &stdMirror : nullptr;
    pl::exporter::standard::exportEntities(world, exportOpts);
    mon.mark("entity export");
  }

  printWindowedSummary(world, transfers, golden.rowsWritten());

  if (streamAml) {
    pg::status("Exporting AML tables...");
    pl::exporter::aml::Options exportOpts;
    exportOpts.piiPools = &pools;
    exportOpts.pgMirror = pgUp ? &amlMirror : nullptr;
    const auto summary = pl::exporter::aml::exportFromArtifacts(
        world, transfers.postedBook.get(), exportOpts,
        amlStream->takeArtifacts());
    printAmlSummary(summary);
    mon.mark("aml export");
  }

  if (streamAmlTxn) {
    pg::status("Exporting AML txn-edges tables (PostgreSQL read-back)...");
    auto artifacts = amlTxnStream->takeArtifacts();

    const auto sarSubjects = pl::exporter::aml::sar::buildSarSubjectIndex(
        world.people.roster.roster, world.people.roster.topology,
        world.holdings.accounts.registry, world.holdings.accounts.ownership);
    const auto sars = pl::exporter::aml::sar::generateSars(
        sarSubjects, artifacts.fraudGroups);

    // The derived bundle needs a second pass over the corpus (sim-end
    // windows); PostgreSQL holds it — read it back in row_seq order.
    pl::postgres::Connection conn{pgConninfo};
    const auto bundle = pl::exporter::aml_txn_edges::readback::buildBundle(
        world.people, world.holdings,
        std::span<const pl::exporter::aml::sar::SarRecord>(sars), conn,
        "transactions");

    pl::exporter::aml_txn_edges::Options exportOpts;
    exportOpts.piiPools = &pools;
    exportOpts.pgMirror = pgUp ? &amlTxnMirror : nullptr;
    const auto summary = pl::exporter::aml_txn_edges::exportFromArtifacts(
        world, transfers.postedBook.get(), exportOpts, std::move(artifacts),
        bundle, std::span<const pl::exporter::aml::sar::SarRecord>(sars));
    printAmlTxnEdgesSummary(summary);
    mon.mark("aml-txn-edges export");
  }

  if (pgUp) {
    std::printf("PostgreSQL: %s tables written directly during the run "
                "(%s)\n",
                std::string{pl::app::name(opts.usecase)}.c_str(),
                directSchemaNote(opts.usecase));
    mon.mark("pg direct tables");
  }

  pg::status("Done.");
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  using namespace ::PhantomLedger;
  namespace pii = synth::pii;
  namespace pg = app::progress;

  try {

    const auto opts = app::cli::parse(argc, argv);

    // Engine selection is automatic (the windowed engine is the proven
    // byte-identical production path for every use case); PL_ENGINE is
    // the test-infrastructure override. The line below makes every run
    // self-describing and lets the gates pin the default.
    const auto engine = app::resolveEngine(opts.engine, opts.usecase);
    std::printf("Engine: %s%s\n", std::string{app::name(engine)}.c_str(),
                opts.engine == app::Engine::automatic ? " (auto)" : "");

    // Backend policy: PostgreSQL is required — probe BEFORE generation
    // and fail fast with the teaching error (resolveBackend exits).
    // PL_FILE_ONLY is the harness escape that keeps the golden and the
    // serverless gates independent of any server.
    const auto backend = resolveBackend(opts);
    const bool pgUp = !backend.fileOnly;

    time::Window window;
    window.start = time::makeTime(opts.startDate);
    window.days = static_cast<int>(opts.days);

    pg::status("Building entity synthesis config...");
    const auto mix = pii::LocaleMix::usBankDefault();
    const auto pools = app::setup::buildPoolSet(opts, mix);
    const auto entityConfig =
        app::setup::buildEntitySynthesis(opts, pools, mix, window.start);

    if (engine == app::Engine::windowed) {
      return runWindowedStream(opts, window, pools, entityConfig,
                               backend.conninfo, pgUp);
    }

    auto rng = random::Rng::fromSeed(opts.seed);

    PhaseMonitor mon;

    pl::pipeline::SimulationResult result;
    {
      pg::Stage genStage("Generating (entities)", 4);
      const auto onPhase = [&](std::string_view phase) {
        mon.mark(phase);
        genStage.tick();
        genStage.setLabel("Generating (" + std::string{phase} + " done)");
      };
      result =
          pipeline::simulate(rng, window, entityConfig, opts.seed, onPhase);
    } // genStage destructor prints trailing newline here

    long long manifestId = -1;

    std::string streamDigest;
    std::uint64_t streamRows = 0;
    {
      const auto &posted = result.transfers.ledger.posted.txns;

      const auto schedule = pipeline::chunk::Schedule::partition(
          window, pipeline::chunk::Strategy{});
      exporter::sinks::Golden golden;

      if (pgUp) {
        pl::postgres::Connection conn{backend.conninfo};
        pl::exporter::sinks::RunLedger ledger{conn};
        ledger.ensureTables();
        // The reference engine also rewrites the shared table (and its
        // runs are never resumable — spans are recorded without
        // digests): supersede any crashed run first.
        ledger.supersedeRunning();
        manifestId = ledger.beginRun(
            pl::exporter::sinks::RunLedger::configHash(
                "monolithic", opts.seed, opts.population, opts.days,
                opts.startDate),
            opts.seed, opts.population, opts.days, opts.startDate);

        exporter::sinks::Postgres pgSink({.conninfo = backend.conninfo});
        pipeline::chunk::Tee tee{golden, pgSink};
        pipeline::chunk::flushPartitioned(
            schedule,
            std::span<const pl::transactions::Transaction>{posted.data(),
                                                           posted.size()},
            tee,
            [&ledger, manifestId](const pipeline::chunk::Span &span,
                                  std::uint64_t rowsInSpan) {
              ledger.recordSpan(manifestId, span.index, rowsInSpan, "");
            });
        std::printf("PostgreSQL: %llu rows -> table 'transactions' "
                    "(%zu spans)\n",
                    static_cast<unsigned long long>(pgSink.rowsWritten()),
                    schedule.size());
      } else {
        pipeline::chunk::flushPartitioned(
            schedule,
            std::span<const pl::transactions::Transaction>{posted.data(),
                                                           posted.size()},
            golden);
      }
      streamDigest = golden.digest();
      streamRows = golden.rowsWritten();
      std::printf("Stream digest: %s  rows: %llu\n", streamDigest.c_str(),
                  static_cast<unsigned long long>(streamRows));
      mon.mark("stream flush");
    }

    pg::status("Exporting...");
    switch (opts.usecase) {
    case app::UseCase::standard: {
      exporter::standard::Options exportOpts;
      exportOpts.piiPools = &pools;
      exportOpts.window = window;
      const pl::exporter::sinks::PgMirror stdMirror{
          .conninfo = backend.conninfo, .schema = "", .tablePrefix = ""};
      exportOpts.pgMirror = pgUp ? &stdMirror : nullptr;
      exporter::standard::exportAll(result, exportOpts);
      printGenericSummary(result);
      break;
    }

    case app::UseCase::muleMl: {
      exporter::mule_ml::Options exportOpts;
      exportOpts.piiPools = &pools;
      const pl::exporter::sinks::PgMirror mlMirror{
          .conninfo = backend.conninfo,
          .schema = "mule_ml",
          .tablePrefix = "ml_ready_"};
      exportOpts.pgMirror = pgUp ? &mlMirror : nullptr;
      exporter::mule_ml::exportAll(result, exportOpts);
      printGenericSummary(result);
      break;
    }

    case app::UseCase::aml: {
      exporter::aml::Options exportOpts;
      exportOpts.piiPools = &pools;
      const pl::exporter::sinks::PgMirror amlMirror{
          .conninfo = backend.conninfo, .schema = "aml", .tablePrefix = "aml_"};
      exportOpts.pgMirror = pgUp ? &amlMirror : nullptr;
      const auto summary = exporter::aml::exportAll(result, exportOpts);
      printAmlSummary(summary);
      break;
    }

    case app::UseCase::amlTxnEdges: {
      exporter::aml_txn_edges::Options exportOpts;
      exportOpts.piiPools = &pools;
      const pl::exporter::sinks::PgMirror amlTxnMirror{
          .conninfo = backend.conninfo,
          .schema = "aml_txn_edges",
          .tablePrefix = "aml_txn_edges_"};
      exportOpts.pgMirror = pgUp ? &amlTxnMirror : nullptr;
      const auto summary =
          exporter::aml_txn_edges::exportAll(result, exportOpts);
      printAmlTxnEdgesSummary(summary);
      break;
    }
    }

    if (pgUp) {
      std::printf("PostgreSQL: %s tables written directly during the run "
                  "(%s)\n",
                  std::string{app::name(opts.usecase)}.c_str(),
                  directSchemaNote(opts.usecase));

      pl::postgres::Connection conn{backend.conninfo};
      pl::exporter::sinks::RunLedger endLedger{conn};
      endLedger.finishRun(manifestId, streamRows, streamDigest);
      mon.mark("pg direct tables");
    }

    mon.mark("export");

    pg::status("Done.");
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
  } catch (...) {
    std::fprintf(stderr, "fatal: unknown exception\n");
    return 1;
  }
}
