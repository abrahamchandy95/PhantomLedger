#pragma once

#include "phantomledger/exporter/aml/edges.hpp"
#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/labels.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <cstddef>
#include <cstdint>

namespace PhantomLedger::exporter::aml {

using Options = ::PhantomLedger::exporter::common::ExportOptions;

struct Summary : ::PhantomLedger::exporter::common::BaseSummary {
  std::size_t chainCount = 0;
  std::size_t shellCount = 0;
};

// Everything the aml export needs from the transaction stream, gathered
// by StreamingAmlExport (streaming.hpp) via the shared per-row functions.
// Bounded: context observations, fraud-scale SAR/chain groups,
// shell-candidate flow aggregates, distinct pair/bank sets, and stream
// statistics — never the corpus.
struct StreamedArtifacts {
  vertices::SharedContext ctx;
  sar::FraudTxnGroups fraudGroups;
  ::PhantomLedger::exporter::labels::ChainGroups chainGroups;
  ::PhantomLedger::exporter::labels::ShellStats shellStats;
  edges::TransactionEdgeSets edgeSets;

  std::int64_t firstTs = 0;
  // H2 step 2c: the corpus MAXIMUM timestamp (the replay-sorted
  // stream's final row), mirroring firstTs — the end-of-window persona
  // resolution reads it in takeArtifacts().
  std::int64_t lastTs = 0;
  std::uint64_t rows = 0;
  std::uint64_t illicitRows = 0;
};

// Writes every table EXCEPT the transaction-streamed ones (transaction
// vertices, the four send/receive edge tables and chain-label edges —
// those come from StreamingAmlExport), using the world, the FINAL
// posted book (account balances), and the accumulated artifacts.
// `world.transfers` is not touched, so the windowed engine's world is
// accepted directly.
[[nodiscard]] Summary
exportFromArtifacts(const ::PhantomLedger::pipeline::SimulationResult &world,
                    const ::PhantomLedger::clearing::Ledger *postedBook,
                    const Options &options, StreamedArtifacts artifacts);

// Corpus form: runs StreamingAmlExport over the retained posted corpus
// (one batch), then exportFromArtifacts — the SAME code path the
// windowed engine uses, so the two engines cannot drift.
[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const Options &options = {});

} // namespace PhantomLedger::exporter::aml
