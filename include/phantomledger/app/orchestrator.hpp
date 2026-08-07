#pragma once

#include "phantomledger/app/backend.hpp"
#include "phantomledger/app/options.hpp"
#include "phantomledger/app/reporting.hpp"
#include "phantomledger/exporter/aml/streaming.hpp"
#include "phantomledger/exporter/aml_txn_edges/streaming.hpp"
#include "phantomledger/exporter/card_fraud/streaming.hpp"
#include "phantomledger/exporter/mule_ml/streaming.hpp"
#include "phantomledger/exporter/sinks/golden.hpp"
#include "phantomledger/exporter/sinks/table_mirror.hpp"
#include "phantomledger/exporter/standard/streaming.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/pipeline/simulate.hpp"
#include "phantomledger/pipeline/stages/entities.hpp"
#include "phantomledger/primitives/random/rng.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/pools.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace PhantomLedger::app::orchestrator {

struct PgMirrors {
  exporter::sinks::PgMirror stdMirror;
  exporter::sinks::PgMirror mlMirror;
  exporter::sinks::PgMirror amlMirror;
  exporter::sinks::PgMirror amlTxnMirror;
  exporter::sinks::PgMirror cfMirror;
};

struct ExporterStreams {
  std::optional<exporter::standard::StreamingTransfersExport> stdStream;
  std::optional<exporter::mule_ml::StreamingMuleMlExport> mlStream;
  std::optional<exporter::aml::StreamingAmlExport> amlStream;
  std::optional<exporter::aml_txn_edges::StreamingAmlTxnEdgesExport>
      amlTxnStream;
  std::optional<exporter::card_fraud::StreamingCardFraudExport> cfStream;
};

class StreamOrchestrator {
public:
  StreamOrchestrator(
      const RunOptions &opts, time::Window window,
      const synth::pii::PoolSet &pools,
      const pipeline::stages::entities::EntitySynthesis &entityConfig,
      const backend::Config &backend);

  [[nodiscard]] int run();

private:
  [[nodiscard]] bool checkPrerequisites() const;
  void buildWorld();
  void initializeSinks();
  void bindStreams();
  void executeFold();
  void rebuildWorldIfNeeded();
  void finalizeExports();

  // --- Internal Helpers ---
  [[nodiscard]] bool isPgUp() const noexcept;

  // Resolves the template deduction error cleanly
  template <typename StreamT> void executeFoldWithStream(StreamT *streamPtr);

  // --- Immutable Configuration ---
  const RunOptions &opts_;
  time::Window window_;
  const synth::pii::PoolSet &pools_;
  const pipeline::stages::entities::EntitySynthesis &entityConfig_;
  const backend::Config &backend_;

  // --- Mutable Orchestration State ---
  random::Rng rng_;
  pipeline::SimulationPipeline pipeline_;
  reporting::PhaseMonitor mon_;

  // --- Evolving Run State ---
  pipeline::SimulationResult world_;
  PgMirrors mirrors_;

  /* The fold's funding declines. DECLARED BEFORE `streams_` on purpose: a
   * card-fraud sink holds this vector's address from `bindStreams()` until its
   * own `finish()`, and members are destroyed in reverse declaration order, so
   * the sink must die first. The fold fills it before that `finish()`. */
  std::vector<transfers::legit::ledger::DeclinedAttempt> declined_;

  ExporterStreams streams_;
  exporter::sinks::Golden golden_;
  pipeline::stages::transfers::WindowedRunResult transfers_;

  // --- Cached Properties ---
  std::uint32_t worldPeopleCount_ = 0;
  std::size_t worldAccountCount_ = 0;
  bool exportPacksReleased_ = false;
};

} // namespace PhantomLedger::app::orchestrator
