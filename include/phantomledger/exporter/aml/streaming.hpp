#pragma once
//
// phantomledger/exporter/aml/streaming.hpp
//
// Streaming half of the aml exporter, shared by BOTH engines: a chunk
// sink that consumes settled rows and
//
//   STREAMS (row-scale, never retained):
//     transaction vertices            (index-carrying writeTransactionRows)
//     send / receive edge tables      (TransactionEdgeClassifier + the
//     cp-send / cp-receive tables      single-row writers)
//     transaction chain-label edges   (index-carrying writer)
//
//   ACCUMULATES (bounded, the shared per-row functions):
//     SharedContext observations      per-account last-txn max
//     SAR fraud groups                fraud-scale copies (sar.hpp)
//     chain groups                    fraud-scale copies (labels.hpp)
//     shell flow aggregates           candidate-scale slots (labels.hpp)
//     transaction-edge sets           distinct pairs / counterparty banks
//     stream stats                    first + last timestamps, row +
//                                     illicit counts
//
// exportFromArtifacts (export.hpp) writes every remaining table from the
// world, the final posted book, and these artifacts. The corpus-based
// exportAll() runs THIS SAME SINK over the retained corpus and then calls
// the same finisher — one code path, two engines, byte-identical tables.
//
// The six streamed tables go through common::Table: when Config::pgMirror
// is armed the rendered bytes stream into PostgreSQL directly — the only
// production destination (no files) — each table on its own connection,
// open across the whole fold. The raw ledger is the streamed
// 'transactions' corpus table itself, never a table here.
//

#include "phantomledger/exporter/aml/edges.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/aml/schema.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/pipeline/data.hpp"
#include "phantomledger/synth/pii/pools.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace PhantomLedger::exporter::aml {

class StreamingAmlExport {
public:
  struct Config {
    const ::PhantomLedger::pipeline::People *people = nullptr;
    const ::PhantomLedger::pipeline::Holdings *holdings = nullptr;
    const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

    // When set, the streamed tables are written directly into
    // PostgreSQL as the bytes the csv::Writer renders — the only
    // production destination.
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

    // Test infrastructure: rendered bytes per table stem.
    common::TableCapture *capture = nullptr;
  };

  explicit StreamingAmlExport(Config config) : config_(std::move(config)) {
    artifacts_.ctx = vertices::buildSharedContextEntities(
        *config_.people, *config_.holdings, *config_.piiPools);
    artifacts_.shellStats = ::PhantomLedger::exporter::labels::initShellStats(
        {.registry = config_.holdings->accounts.registry,
         .ownership = config_.holdings->accounts.ownership,
         .topology = config_.people->roster.topology});
    classifier_.emplace(artifacts_.ctx);

    // Direct-table mirrors keep the historical tree naming
    // (aml_vertices_<stem> / aml_edges_<stem> in the target schema).
    if (config_.pgMirror != nullptr) {
      vtxMirror_.emplace(sinks::PgMirror{
          .conninfo = config_.pgMirror->conninfo,
          .schema = config_.pgMirror->schema,
          .tablePrefix = config_.pgMirror->tablePrefix + "vertices_"});
      edgeMirror_.emplace(sinks::PgMirror{
          .conninfo = config_.pgMirror->conninfo,
          .schema = config_.pgMirror->schema,
          .tablePrefix = config_.pgMirror->tablePrefix + "edges_"});
    }
    const common::TableTarget vtxTarget{
        .pg = vtxMirror_.has_value() ? &*vtxMirror_ : nullptr,
        .capture = config_.capture};
    const common::TableTarget edgeTarget{
        .pg = edgeMirror_.has_value() ? &*edgeMirror_ : nullptr,
        .capture = config_.capture};

    namespace amlSchema = ::PhantomLedger::exporter::schema::aml;
    txnW_.emplace(common::openTable(vtxTarget, amlSchema::kTransaction));
    sendW_.emplace(common::openTable(edgeTarget, amlSchema::kSendTransaction));
    recvW_.emplace(
        common::openTable(edgeTarget, amlSchema::kReceiveTransaction));
    cpSendW_.emplace(common::openTable(
        edgeTarget, amlSchema::kCounterpartySendTransaction));
    cpRecvW_.emplace(common::openTable(
        edgeTarget, amlSchema::kCounterpartyReceiveTransaction));
    chainLabelW_.emplace(
        common::openTable(edgeTarget, amlSchema::kTransactionChainLabel));

    emitter_.send_ = &sendW_->writer();
    emitter_.recv_ = &recvW_->writer();
    emitter_.cpSend_ = &cpSendW_->writer();
    emitter_.cpRecv_ = &cpRecvW_->writer();
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
    // ... and the last row of the last batch carries the maximum (H2
    // step 2c: the end-of-window persona resolution anchor).
    artifacts_.lastTs = txnsBatch.back().timestamp;
    artifacts_.rows += txnsBatch.size();

    vertices::writeTransactionRows(*txnW_, txnsBatch, txnIndex_);
    ::PhantomLedger::exporter::labels::writeTransactionChainLabelRows(
        *chainLabelW_, txnsBatch, chainLabelIndex_);

    for (const auto &tx : txnsBatch) {
      classifier_->observe(tx, emitter_);
      vertices::observeTransaction(artifacts_.ctx, tx);
      sar::accumulateFraudTxn(artifacts_.fraudGroups, tx);
      ::PhantomLedger::exporter::labels::accumulateChainTxn(
          artifacts_.chainGroups, tx);
      ::PhantomLedger::exporter::labels::accumulateShellTxn(
          artifacts_.shellStats, tx);
      if (tx.fraud.flag != 0) {
        ++artifacts_.illicitRows;
      }
    }
  }

  void endSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void finish() {
    artifacts_.edgeSets = classifier_->takeSets();

    txnW_.reset();
    sendW_.reset();
    recvW_.reset();
    cpSendW_.reset();
    cpRecvW_.reset();
    chainLabelW_.reset();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return artifacts_.rows;
  }

  // Call after finish(); the classifier and writers are done with the
  // context by then. H2 step 2c: the Customer persona resolves to the
  // corpus-end state here — after the stream closed, before the
  // finisher writes the entity tables — so both engines resolve from
  // the identical (lastTs, rows) pair.
  [[nodiscard]] StreamedArtifacts takeArtifacts() noexcept {
    vertices::resolveEndOfWindowPersonas(artifacts_.ctx,
                                         config_.people->personas,
                                         artifacts_.lastTs, artifacts_.rows);
    return std::move(artifacts_);
  }

private:
  struct CsvEdgeEmitter final : edges::TransactionEdgeEmitter {
    void send(const entity::Key &acct, std::size_t idx1) override {
      edges::writeAcctTxnRow(*send_, acct, idx1);
    }
    void receive(const entity::Key &acct, std::size_t idx1) override {
      edges::writeAcctTxnRow(*recv_, acct, idx1);
    }
    void cpSend(const entity::Key &cp, std::size_t idx1,
                const std::string &name) override {
      edges::writeCpTxnRow(*cpSend_, cp, idx1, name);
    }
    void cpReceive(const entity::Key &cp, std::size_t idx1,
                   const std::string &name) override {
      edges::writeCpTxnRow(*cpRecv_, cp, idx1, name);
    }

    csv::Writer *send_ = nullptr;
    csv::Writer *recv_ = nullptr;
    csv::Writer *cpSend_ = nullptr;
    csv::Writer *cpRecv_ = nullptr;
  };

  Config config_;

  StreamedArtifacts artifacts_;
  std::optional<edges::TransactionEdgeClassifier> classifier_;
  CsvEdgeEmitter emitter_;

  std::optional<sinks::PgMirror> vtxMirror_;
  std::optional<sinks::PgMirror> edgeMirror_;

  std::optional<common::Table> txnW_;
  std::optional<common::Table> sendW_;
  std::optional<common::Table> recvW_;
  std::optional<common::Table> cpSendW_;
  std::optional<common::Table> cpRecvW_;
  std::optional<common::Table> chainLabelW_;

  std::size_t txnIndex_ = 1;
  std::size_t chainLabelIndex_ = 1;
};

} // namespace PhantomLedger::exporter::aml
