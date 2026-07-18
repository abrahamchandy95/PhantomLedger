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
//     transactions.csv (optional)     (the shared ledger writer)
//
//   ACCUMULATES (bounded, the shared per-row functions):
//     SharedContext observations      per-account last-txn max
//     SAR fraud groups                fraud-scale copies (sar.hpp)
//     chain groups                    fraud-scale copies (labels.hpp)
//     shell flow aggregates           candidate-scale slots (labels.hpp)
//     transaction-edge sets           distinct pairs / counterparty banks
//     stream stats                    first timestamp, row + illicit counts
//
// exportFromArtifacts (export.hpp) writes every remaining table from the
// world, the final posted book, and these artifacts. The corpus-based
// exportAll() runs THIS SAME SINK over the retained corpus and then calls
// the same finisher — one code path, two engines, byte-identical files.
//
// CSV retirement arc: the six transaction-streamed tables go through
// common::Table, so when Config::pgMirror is armed the same bytes
// stream into PostgreSQL directly, each table on its own connection,
// open across the whole fold. An EMPTY Config::outDir disables the
// file leg (5b): the composed aml/{vertices,edges} subdirectories
// must then never reach a TableTarget, or they would resolve as
// relative paths in the working directory. The raw ledger CSV stays
// FILE-ONLY — its stem is "transactions", the streamed corpus table's
// name, and the canonical stream must never be overwritten by its
// dump.
//

#include "phantomledger/exporter/aml/edges.hpp"
#include "phantomledger/exporter/aml/export.hpp"
#include "phantomledger/exporter/aml/schema.hpp"
#include "phantomledger/exporter/aml/vertices.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
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

namespace PhantomLedger::exporter::aml {

class StreamingAmlExport {
public:
  struct Config {
    const ::PhantomLedger::pipeline::People *people = nullptr;
    const ::PhantomLedger::pipeline::Holdings *holdings = nullptr;
    const ::PhantomLedger::synth::pii::PoolSet *piiPools = nullptr;

    // The run output directory; tables land under <outDir>/aml/{vertices,
    // edges} and transactions.csv (when enabled) at <outDir>, exactly
    // like exportAll. Empty => no files (PG-only run).
    std::filesystem::path outDir;
    bool showTransactions = false;

    // When set, the streamed tables are ALSO written directly into
    // PostgreSQL as the same bytes the CSV files receive (the ledger
    // CSV is deliberately excluded; see file comment).
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;
  };

  explicit StreamingAmlExport(Config config) : config_(std::move(config)) {
    artifacts_.ctx = vertices::buildSharedContextEntities(
        *config_.people, *config_.holdings, *config_.piiPools);
    artifacts_.shellStats = ::PhantomLedger::exporter::labels::initShellStats(
        {.registry = config_.holdings->accounts.registry,
         .ownership = config_.holdings->accounts.ownership,
         .topology = config_.people->roster.topology});
    classifier_.emplace(artifacts_.ctx);

    const bool files = !config_.outDir.empty();
    const auto vtxDir =
        files ? config_.outDir / "aml" / "vertices" : std::filesystem::path{};
    const auto edgeDir =
        files ? config_.outDir / "aml" / "edges" : std::filesystem::path{};
    if (files) {
      std::filesystem::create_directories(vtxDir);
      std::filesystem::create_directories(edgeDir);
    }

    // Direct-table mirrors reproduce the csv_loader tree naming
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
        .dir = vtxDir,
        .pg = vtxMirror_.has_value() ? &*vtxMirror_ : nullptr};
    const common::TableTarget edgeTarget{
        .dir = edgeDir,
        .pg = edgeMirror_.has_value() ? &*edgeMirror_ : nullptr};

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
    if (config_.showTransactions && files) {
      // File-only: stem "transactions" is the streamed corpus table.
      const common::TableTarget fileOnly{.dir = config_.outDir, .pg = nullptr};
      ledgerW_.emplace(common::openTable(
          fileOnly, ::PhantomLedger::exporter::schema::kLedger));
    }

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
    artifacts_.rows += txnsBatch.size();

    vertices::writeTransactionRows(*txnW_, txnsBatch, txnIndex_);
    ::PhantomLedger::exporter::labels::writeTransactionChainLabelRows(
        *chainLabelW_, txnsBatch, chainLabelIndex_);
    if (ledgerW_.has_value()) {
      common::writeLedgerRows(*ledgerW_, txnsBatch);
    }

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
    ledgerW_.reset();
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept {
    return artifacts_.rows;
  }

  // Call after finish(); the classifier and writers are done with the
  // context by then.
  [[nodiscard]] StreamedArtifacts takeArtifacts() noexcept {
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
  std::optional<common::Table> ledgerW_;

  std::size_t txnIndex_ = 1;
  std::size_t chainLabelIndex_ = 1;
};

} // namespace PhantomLedger::exporter::aml
