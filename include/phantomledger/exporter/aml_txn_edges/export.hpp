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
#include <vector>

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

// Everything generation produced for this use case — the ONE handoff
// between the engine side (fold + post-fold derivation) and the table
// writer. Both engine paths assemble it the same way: take the sink's
// artifacts, generate SARs from the accumulated groups
// (aml::sar::generateSars world form), build the bundle (readback for
// the windowed engine, derived for the corpus reference). A NEW fold
// or derivation output belongs IN here, never as a new
// exportFromProducts parameter — that is the rule that keeps the
// finisher's signature flat as the model grows.
struct StreamProducts {
  StreamedArtifacts artifacts;
  derived::Bundle bundle;
  std::vector<aml::sar::SarRecord> sars;
};

// Writes every table EXCEPT the two the sink streamed (TRANSACTED,
// TRANSACTION_CHAIN_LABEL), from the world + posted book + the
// assembled stream products. Shared by both engines.
[[nodiscard]] Summary
exportFromProducts(const ::PhantomLedger::pipeline::SimulationResult &world,
                   const ::PhantomLedger::clearing::Ledger *postedBook,
                   const Options &options, StreamProducts products);

// One code path, two engines: runs the streaming sink over the retained
// corpus as one batch, assembles StreamProducts (SARs from the
// accumulated groups, bundle from the corpus), then calls the same
// finisher.
[[nodiscard]] Summary
exportAll(const ::PhantomLedger::pipeline::SimulationResult &result,
          const Options &options = {});

} // namespace PhantomLedger::exporter::aml_txn_edges
