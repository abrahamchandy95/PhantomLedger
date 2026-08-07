#include "phantomledger/app/orchestrator.hpp"

#include "phantomledger/app/progress.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml_txn_edges/export.hpp"
#include "phantomledger/exporter/aml_txn_edges/readback.hpp"
#include "phantomledger/exporter/card_fraud/export.hpp"
#include "phantomledger/exporter/econ/export.hpp"
#include "phantomledger/exporter/sinks/postgres.hpp"
#include "phantomledger/exporter/sinks/run_ledger.hpp"
#include "phantomledger/exporter/standard/export.hpp"
#include "phantomledger/pipeline/chunk/sink.hpp"
#include "phantomledger/synth/personas/join.hpp"

#include <print>
#include <string>
#include <string_view>

namespace PhantomLedger::app::orchestrator {

StreamOrchestrator::StreamOrchestrator(
    const RunOptions &opts, time::Window window,
    const synth::pii::PoolSet &pools,
    const pipeline::stages::entities::EntitySynthesis &entityConfig,
    const backend::Config &backend)
    : opts_(opts), window_(window), pools_(pools), entityConfig_(entityConfig),
      backend_(backend), rng_(random::Rng::fromSeed(opts.seed)),
      pipeline_(rng_, window, entityConfig, opts.seed) {}

int StreamOrchestrator::run() {
  if (!checkPrerequisites()) {
    return 1;
  }

  buildWorld();
  initializeSinks();
  bindStreams();
  executeFold();
  rebuildWorldIfNeeded();
  finalizeExports();

  return 0;
}

bool StreamOrchestrator::isPgUp() const noexcept {
  return !backend_.isFileOnly;
}

bool StreamOrchestrator::checkPrerequisites() const {
  if (opts_.usecase == UseCase::amlTxnEdges && !isPgUp()) {
    std::println(
        stderr,
        "fatal: --usecase aml-txn-edges requires PostgreSQL: the derived "
        "analytics (alerts, cases, 30/90-day aggregates) read the "
        "streamed corpus back from the transactions table. There is no "
        "serverless mode for this use case — unset PL_FILE_ONLY.");
    return false;
  }
  return true;
}

void StreamOrchestrator::buildWorld() {
  progress::Stage genStage("Generating (windowed: world)", 3);

  const auto onPhase = [&](std::string_view phase) {
    mon_.mark(phase);
    genStage.tick();
    genStage.setLabel("Generating (" + std::string{phase} + " done)");
  };

  world_ = pipeline_.buildWorld(onPhase);
  worldPeopleCount_ = world_.people.roster.roster.count;
  worldAccountCount_ = world_.holdings.accounts.registry.records.size();
}

void StreamOrchestrator::initializeSinks() {
  const std::string &conn = backend_.conninfo;

  mirrors_ = PgMirrors{
      .stdMirror = {.conninfo = conn, .schema = "", .tablePrefix = ""},
      .mlMirror = {.conninfo = conn,
                   .schema = "mule_ml",
                   .tablePrefix = "ml_ready_"},
      .amlMirror = {.conninfo = conn, .schema = "aml", .tablePrefix = "aml_"},
      .amlTxnMirror = {.conninfo = conn,
                       .schema = "aml_txn_edges",
                       .tablePrefix = "aml_txn_edges_"},
      .cfMirror = {.conninfo = conn,
                   .schema = "card_fraud",
                   .tablePrefix = "cf_"},
  };

  if (isPgUp()) {
    const exporter::sinks::PgMirror econMirror{
        .conninfo = backend_.conninfo, .schema = "econ", .tablePrefix = ""};
    exporter::econ::writeEraTables({.pg = &econMirror, .capture = nullptr});
    mon_.mark("econ era tables");
  }
}

void StreamOrchestrator::bindStreams() {
  exportPacksReleased_ = (opts_.usecase != UseCase::muleMl);

  switch (opts_.usecase) {
  case UseCase::standard:
    streams_.stdStream.emplace(
        exporter::standard::StreamingTransfersExport::Config{
            .registry = &world_.holdings.accounts.registry,
            .lookup = &world_.holdings.accounts.lookup,
            .membership = synth::personas::join_cohort::membershipOf(
                world_.people.personas, window_),
            .window = window_,
            .pgMirror = isPgUp() ? &mirrors_.stdMirror : nullptr,
        });
    break;

  case UseCase::muleMl:
    streams_.mlStream.emplace(exporter::mule_ml::StreamingMuleMlExport::Config{
        .registry = &world_.holdings.accounts.registry,
        .roster = &world_.people.roster.roster,
        .pii = &world_.people.pii,
        .devices = &world_.infra.devices,
        .ips = &world_.infra.ips,
        .piiPools = &pools_,
        .pgMirror = isPgUp() ? &mirrors_.mlMirror : nullptr,
    });
    break;

  case UseCase::aml:
    streams_.amlStream.emplace(exporter::aml::StreamingAmlExport::Config{
        .people = &world_.people,
        .holdings = &world_.holdings,
        .piiPools = &pools_,
        .pgMirror = isPgUp() ? &mirrors_.amlMirror : nullptr,
    });
    break;

  case UseCase::amlTxnEdges:
    streams_.amlTxnStream.emplace(
        exporter::aml_txn_edges::StreamingAmlTxnEdgesExport::Config{
            .people = &world_.people,
            .holdings = &world_.holdings,
            .piiPools = &pools_,
            .pgMirror = isPgUp() ? &mirrors_.amlTxnMirror : nullptr,
        });
    break;

  case UseCase::cardFraud:
    streams_.cfStream.emplace(
        exporter::card_fraud::StreamingCardFraudExport::Config{
            .registry = &world_.holdings.accounts.registry,
            .lookup = &world_.holdings.accounts.lookup,
            .membership = synth::personas::join_cohort::membershipOf(
                world_.people.personas, window_),
            .cards = &world_.holdings.creditCards,
            .merchants = &world_.counterparties.merchants,
            .pgMirror = isPgUp() ? &mirrors_.cfMirror : nullptr,
            .window = window_,
            /* Empty now; the fold fills it before the sink's finish(). */
            .declined = &declined_,
        });
    break;
  }
}

void StreamOrchestrator::executeFold() {
  if (exportPacksReleased_) {
    pipeline::releaseExportOnlyPacks(world_);
    mon_.mark("export-only packs freed");
  }

  // Branch explicitly to satisfy C++ template strictness
  if (opts_.usecase == UseCase::standard) {
    executeFoldWithStream(streams_.stdStream ? &*streams_.stdStream : nullptr);
  } else if (opts_.usecase == UseCase::muleMl) {
    executeFoldWithStream(streams_.mlStream ? &*streams_.mlStream : nullptr);
  } else if (opts_.usecase == UseCase::aml) {
    executeFoldWithStream(streams_.amlStream ? &*streams_.amlStream : nullptr);
  } else if (opts_.usecase == UseCase::amlTxnEdges) {
    executeFoldWithStream(streams_.amlTxnStream ? &*streams_.amlTxnStream
                                                : nullptr);
  } else if (opts_.usecase == UseCase::cardFraud) {
    executeFoldWithStream(streams_.cfStream ? &*streams_.cfStream : nullptr);
  } else {
    executeFoldWithStream<exporter::standard::StreamingTransfersExport>(
        nullptr);
  }

  std::println("Stream digest: {}  rows: {}", golden_.digest(),
               golden_.rowsWritten());
  mon_.mark("stream flush");
}

template <typename StreamT>
void StreamOrchestrator::executeFoldWithStream(StreamT *streamPtr) {
  progress::Stage foldStage("Generating (windowed: transfers)", 1);
  const auto onPhase = [&](std::string_view phase) {
    mon_.mark(phase);
    foldStage.tick();
  };

  /* ONE options object for every branch below, because the branches differ in
   * SINK and must not differ in fold behaviour. `declined` is what the
   * card-fraud sink already points at; the other use cases fill it and ignore
   * it, which costs one small vector and keeps the fold identical across
   * them. */
  pipeline::stages::transfers::WindowedRunOptions foldOpts{};
  foldOpts.declined = &declined_;

  if (!isPgUp()) {
    if (streamPtr) {
      pipeline::chunk::Tee tee{golden_, *streamPtr};
      transfers_ = pipeline_.runWindowedTransfers(world_, tee, foldOpts,
                                                  onPhase);
    } else {
      transfers_ =
          pipeline_.runWindowedTransfers(world_, golden_, foldOpts, onPhase);
    }
    return;
  }

  postgres::Connection conn{backend_.conninfo};
  exporter::sinks::RunLedger ledger{conn};
  ledger.ensureTables();

  const auto configHash = exporter::sinks::RunLedger::configHash(
      "windowed", opts_.seed, opts_.population, opts_.days, opts_.startDate);

  auto plan = ledger.findResumable(configHash);
  if (plan.has_value() && !ledger.prepareResume(*plan, "transactions")) {
    std::println(stderr,
                 "warning: interrupted run #{} is not resumable; marking "
                 "failed and rewriting",
                 plan->manifestId);
    ledger.markFailed(plan->manifestId);
    plan.reset();
  }

  long long pgManifestId = -1;
  if (plan.has_value()) {
    pgManifestId = plan->manifestId;
    std::println("Resuming run #{}: {} spans ({} rows) already durable",
                 plan->manifestId, plan->spans.size(), plan->rows);
  } else {
    ledger.supersedeRunning();
    pgManifestId = ledger.beginRun(configHash, opts_.seed, opts_.population,
                                   opts_.days, opts_.startDate);
  }

  exporter::sinks::Postgres pgSink(
      {.conninfo = backend_.conninfo,
       .createTable = !plan.has_value(),
       .truncateFirst = !plan.has_value(),
       .startRowSeq = plan.has_value() ? plan->rows : 0});

  exporter::sinks::GatedSink<exporter::sinks::Postgres> gatedPg{.inner =
                                                                    &pgSink};
  pipeline::chunk::Tee teePg{golden_, gatedPg};
  const exporter::sinks::ResumePlan *planPtr =
      plan.has_value() ? &*plan : nullptr;

  if (streamPtr) {
    pipeline::chunk::Tee teeAll{teePg, *streamPtr};
    exporter::sinks::ResumableSpanSink<decltype(teeAll)> sink{
        {.inner = &teeAll,
         .copyGate = &gatedPg.open,
         .ledger = &ledger,
         .manifestId = pgManifestId,
         .plan = planPtr,
         .conninfo = backend_.conninfo}};

    transfers_ = pipeline_.runWindowedTransfers(world_, sink, foldOpts, onPhase);

    if (sink.spansSkipped() > 0) {
      std::println("PostgreSQL: {} rows total -> table 'transactions' ({} "
                   "spans copied, {} skipped)",
                   pgSink.rowsWritten(), sink.spansWritten(),
                   sink.spansSkipped());
    } else {
      std::println(
          "PostgreSQL: {} rows -> table 'transactions' ({} spans, streamed)",
          pgSink.rowsWritten(), sink.spansWritten());
    }
  } else {
    exporter::sinks::ResumableSpanSink<decltype(teePg)> sink{
        {.inner = &teePg,
         .copyGate = &gatedPg.open,
         .ledger = &ledger,
         .manifestId = pgManifestId,
         .plan = planPtr,
         .conninfo = backend_.conninfo}};

    transfers_ = pipeline_.runWindowedTransfers(world_, sink, foldOpts, onPhase);

    if (sink.spansSkipped() > 0) {
      std::println("PostgreSQL: {} rows total -> table 'transactions' ({} "
                   "spans copied, {} skipped)",
                   pgSink.rowsWritten(), sink.spansWritten(),
                   sink.spansSkipped());
    } else {
      std::println(
          "PostgreSQL: {} rows -> table 'transactions' ({} spans, streamed)",
          pgSink.rowsWritten(), sink.spansWritten());
    }
  }
}

void StreamOrchestrator::rebuildWorldIfNeeded() {
  if (!exportPacksReleased_)
    return;

  world_ = pipeline::SimulationResult{};
  progress::Stage rebuildStage("Rebuilding world (vertex export)", 3);

  const auto onRebuildPhase = [&](std::string_view phase) {
    mon_.mark("rebuild " + std::string{phase});
    rebuildStage.tick();
  };

  world_ = pipeline_.rebuildWorldForExport(onRebuildPhase);
}

void StreamOrchestrator::finalizeExports() {
  namespace pg = app::progress;

  if (opts_.usecase == UseCase::standard) {
    pg::status("Exporting entity tables...");
    exporter::standard::Options exportOpts;
    exportOpts.piiPools = &pools_;
    exportOpts.window = window_;
    exportOpts.pgMirror = isPgUp() ? &mirrors_.stdMirror : nullptr;
    exporter::standard::exportEntities(world_, exportOpts);
    mon_.mark("entity export");
  }

  reporting::windowedSummary(worldPeopleCount_, worldAccountCount_, transfers_,
                             golden_.rowsWritten());

  if (streams_.amlStream) {
    pg::status("Exporting AML tables...");
    exporter::aml::Options exportOpts;
    exportOpts.piiPools = &pools_;
    exportOpts.pgMirror = isPgUp() ? &mirrors_.amlMirror : nullptr;
    const auto summary = exporter::aml::exportFromArtifacts(
        world_, transfers_.postedBook.get(), exportOpts,
        streams_.amlStream->takeArtifacts());
    reporting::amlSummary(summary);
    mon_.mark("aml export");
  }

  if (streams_.amlTxnStream) {
    pg::status("Exporting AML txn-edges tables (PostgreSQL read-back)...");
    auto artifacts = streams_.amlTxnStream->takeArtifacts();
    auto sars = exporter::aml::sar::generateSars(world_.people, world_.holdings,
                                                 artifacts.fraudGroups);

    postgres::Connection conn{backend_.conninfo};
    auto bundle = exporter::aml_txn_edges::readback::buildBundle(
        world_.people, world_.holdings,
        std::span<const exporter::aml::sar::SarRecord>(sars), conn,
        "transactions");

    exporter::aml_txn_edges::Options exportOpts;
    exportOpts.piiPools = &pools_;
    exportOpts.pgMirror = isPgUp() ? &mirrors_.amlTxnMirror : nullptr;
    const auto summary = exporter::aml_txn_edges::exportFromProducts(
        world_, transfers_.postedBook.get(), exportOpts,
        {.artifacts = std::move(artifacts),
         .bundle = std::move(bundle),
         .sars = std::move(sars)});
    reporting::amlTxnEdgesSummary(summary);
    mon_.mark("aml-txn-edges export");
  }

  if (streams_.cfStream) {
    pg::status("Exporting card-fraud tables (TF_GNN_v3)...");
    exporter::card_fraud::Options exportOpts;
    exportOpts.piiPools = &pools_;
    exportOpts.window = window_;
    exportOpts.pgMirror = isPgUp() ? &mirrors_.cfMirror : nullptr;
    const auto summary = exporter::card_fraud::exportFromArtifacts(
        world_, exportOpts, streams_.cfStream->takeArtifacts());
    reporting::cardFraudSummary(summary);
    mon_.mark("card-fraud export");
  }

  if (isPgUp()) {
    postgres::Connection conn{backend_.conninfo};
    exporter::sinks::RunLedger ledger{conn};
    const auto configHash = exporter::sinks::RunLedger::configHash(
        "windowed", opts_.seed, opts_.population, opts_.days, opts_.startDate);
    if (auto plan = ledger.findResumable(configHash)) {
      ledger.finishRun(plan->manifestId, golden_.rowsWritten(),
                       golden_.digest());
    }
    std::println("PostgreSQL: {} tables written directly during the run ({})",
                 app::name(opts_.usecase), backend::schemaName(opts_.usecase));
    mon_.mark("pg direct tables");
  }

  pg::status("Done.");
}

} // namespace PhantomLedger::app::orchestrator
