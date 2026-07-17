#pragma once
//
// phantomledger/exporter/aml_txn_edges/streaming.hpp
//
// Streaming half of the aml-txn-edges exporter, shared by BOTH engines:
// a chunk sink that consumes settled rows and
//
//   STREAMS (row-scale, never retained):
//     TRANSACTED edges                (index-carrying writeTransactedRows;
//                                      the 1-based id IS row_seq)
//     TRANSACTION_CHAIN_LABEL edges   (index-carrying labels writer)
//
//   ACCUMULATES (bounded, the shared per-row functions):
//     SharedContext observations      per-account last-txn max (aml seam)
//     SAR fraud groups                fraud-scale copies (aml seam)
//     chain groups                    fraud-scale copies (labels seam)
//     shell flow aggregates           candidate-scale slots (labels seam)
//     involves-counterparty pairs     pair-scale set (edges seam)
//     fraud rows by index             fraud-scale FULL copies — the
//                                     promoted-txn writers need amount,
//                                     timestamp and channel
//     stream stats                    first timestamp, row + illicit counts
//
// The DERIVED BUNDLE is deliberately NOT built here: its 30/90-day
// sim-end-relative windows need the corpus end before the sweep, which a
// single forward pass cannot know. The windowed caller builds it from
// PostgreSQL (readback::buildBundle) after the fold; the corpus caller
// builds it in memory (derived::buildBundle). Their parity is pinned by
// test_derived_readback. exportFromArtifacts (export.hpp) then writes
// every remaining table. The corpus-based exportAll() runs THIS SAME
// SINK over the retained corpus and calls the same finisher — one code
// path, two engines, byte-identical files.
//
// CSV retirement arc: both streamed tables go through common::Table, so
// when Config::pgMirror is armed the same bytes stream into PostgreSQL
// directly, each on its own connection, open across the whole fold.
//

#include "phantomledger/exporter/aml/sar.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/aml_txn_edges/edges.hpp"
#include "phantomledger/exporter/aml_txn_edges/export.hpp"
#include "phantomledger/exporter/aml_txn_edges/schema.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/labels.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>

namespace PhantomLedger::exporter::aml_txn_edges {

class StreamingAmlTxnEdgesExport {
public:
  struct Config {
    const ::PhantomLedger::pipeline::People *people = nullptr;
    const ::PhantomLedger::pipeline::Holdings *holdings = nullptr;
    const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

    // The run output directory; the streamed tables land under
    // <outDir>/aml_txn_edges/edges, exactly like exportAll.
    std::filesystem::path outDir;

    // When set, the streamed tables are ALSO written directly into
    // PostgreSQL as the same bytes the CSV files receive.
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;
  };

  explicit StreamingAmlTxnEdgesExport(Config config)
      : config_(std::move(config)) {
    artifacts_.ctx = ::PhantomLedger::exporter::aml::vertices::
        buildSharedContextEntities(*config_.people, *config_.holdings,
                                   *config_.piiPools);
    artifacts_.shellStats = ::PhantomLedger::exporter::labels::initShellStats(
        {.registry = config_.holdings->accounts.registry,
         .ownership = config_.holdings->accounts.ownership,
         .topology = config_.people->roster.topology});

    const auto edgeDir = config_.outDir / "aml_txn_edges" / "edges";
    std::filesystem::create_directories(edgeDir);

    // Direct-table mirror reproduces the csv_loader tree naming
    // (aml_txn_edges_edges_<stem> in the target schema).
    if (config_.pgMirror != nullptr) {
      edgeMirror_.emplace(sinks::PgMirror{
          .conninfo = config_.pgMirror->conninfo,
          .schema = config_.pgMirror->schema,
          .tablePrefix = config_.pgMirror->tablePrefix + "edges_"});
    }
    const common::TableTarget edgeTarget{
        .dir = edgeDir,
        .pg = edgeMirror_.has_value() ? &*edgeMirror_ : nullptr};

    namespace sch = ::PhantomLedger::exporter::schema::aml_txn_edges;
    transactedW_.emplace(common::openTable(edgeTarget, sch::kTransacted));
    chainLabelW_.emplace(
        common::openTable(edgeTarget, sch::kTransactionChainLabel));
  }

  void beginSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txnsBatch) {
    if (txnsBatch.empty()) {
      return;
    }
    if (artifacts_.rows == 0) {
      // The stream is replay-sorted, so the first row carries the minimum
      // timestamp — identical to deriveSimStart over the full corpus.
      artifacts_.firstTs = txnsBatch.front().timestamp;
    }
    artifacts_.rows += txnsBatch.size();

    edges::writeTransactedRows(*transactedW_, txnsBatch, transactedIndex_);
    ::PhantomLedger::exporter::labels::writeTransactionChainLabelRows(
        *chainLabelW_, txnsBatch, chainLabelIndex_);

    for (const auto &tx : txnsBatch) {
      ++rowCounter_;
      ::PhantomLedger::exporter::aml::vertices::observeTransaction(
          artifacts_.ctx, tx);
      ::PhantomLedger::exporter::aml::sar::accumulateFraudTxn(
          artifacts_.fraudGroups, tx);
      ::PhantomLedger::exporter::labels::accumulateChainTxn(
          artifacts_.chainGroups, tx);
      ::PhantomLedger::exporter::labels::accumulateShellTxn(
          artifacts_.shellStats, tx);
      edges::accumulateInvolvesCounterparty(artifacts_.cpPairs, tx);
      if (tx.fraud.flag != 0) {
        artifacts_.fraudTxns.emplace(static_cast<std::size_t>(rowCounter_),
                                     tx);
        ++artifacts_.illicitRows;
      }
    }
  }

  void endSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void finish() {
    transactedW_.reset();
    chainLabelW_.reset();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return artifacts_.rows;
  }

  // Call after finish(); the writers are done by then.
  [[nodiscard]] StreamedArtifacts takeArtifacts() noexcept {
    return std::move(artifacts_);
  }

private:
  Config config_;

  StreamedArtifacts artifacts_;

  std::optional<sinks::PgMirror> edgeMirror_;

  std::optional<common::Table> transactedW_;
  std::optional<common::Table> chainLabelW_;

  std::size_t transactedIndex_ = 1;
  std::size_t chainLabelIndex_ = 1;
  std::uint64_t rowCounter_ = 0;
};

} // namespace PhantomLedger::exporter::aml_txn_edges
