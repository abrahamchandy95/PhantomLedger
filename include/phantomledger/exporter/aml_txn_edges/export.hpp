#pragma once

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/exporter/aml_txn_edges/edges.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/labels.hpp"
#include "phantomledger/pipeline/result.hpp"
#include "phantomledger/transactions/clearing/ledger.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace PhantomLedger::exporter::aml_txn_edges {

using Options = ::PhantomLedger::exporter::common::ExportOptions;

struct Summary : ::PhantomLedger::exporter::common::BaseSummary {
  std::size_t alertCount = 0;
  std::size_t ctrCount = 0;
  std::size_t caseCount = 0;
  std::size_t businessCount = 0;
  std::size_t flowAggEdgeCount = 0;
  std::size_t linkCommEdgeCount = 0;
  std::size_t chainCount = 0;
  std::size_t shellCount = 0;
};

// Everything StreamingAmlTxnEdgesExport accumulates while the streamed
// tables are written during the fold. The derived Bundle is NOT here by
// design: its sim-end-relative windows need a second pass over the
// corpus, so the windowed caller builds it from PostgreSQL
// (readback::buildBundle) and the corpus caller from the retained
// vector (derived::buildBundle) — parity pinned by
// test_derived_readback.
struct StreamedArtifacts {
  aml::vertices::SharedContext ctx;
  aml::sar::FraudTxnGroups fraudGroups;
  labels::ChainGroups chainGroups;
  labels::ShellStats shellStats;
  edges::AcctCpPairs cpPairs;
  derived::FraudTxnByIndex fraudTxns;
  std::int64_t firstTs = common::kFallbackEpoch;
  std::uint64_t rows = 0;
  std::uint64_t illicitRows = 0;
};

// Writes every table EXCEPT the two the sink streamed (TRANSACTED,
// TRANSACTION_CHAIN_LABEL), from the world + posted book + artifacts +
// the derived bundle + SARs. Shared by both engines.
[[nodiscard]] Summary exportFromArtifacts(
    const ::PhantomLedger::pipeline::SimulationResult &world,
    const ::PhantomLedger::clearing::Ledger *postedBook, const Options &options,
    StreamedArtifacts artifacts, const derived::Bundle &bundle,
    std::span<const aml::sar::SarRecord> sars);

// One code path, two engines: runs the streaming sink over the retained
// corpus as one batch, builds SARs from the accumulated groups and the
// bundle from the corpus, then calls the same finisher.
[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const Options &options = {});

} // namespace PhantomLedger::exporter::aml_txn_edges
