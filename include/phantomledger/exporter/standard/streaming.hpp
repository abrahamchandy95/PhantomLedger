#pragma once
//
// phantomledger/exporter/standard/streaming.hpp
//
// Streaming twin of the standard exporter's transaction-scale artifacts,
// for the windowed engine: a chunk sink that consumes settled spans as
// Phase B folds them and produces byte-identical files to exportAll()'s
// corpus-based path:
//
//   has_paid            per-(source,target) aggregate — accumulated row
//                       by row in corpus order (so the floating-point
//                       sums are bit-identical), written SORTED at
//                       finish(), exactly like writeHasPaidRows
//   account_flow_agg    same, with fixed-width temporal bins
//   transactions (opt)  raw ledger rows, streamed as they arrive
//
// The membership filter is applied per row (the same activeAt predicate
// filterByMembership uses), so no visible-corpus copy is ever
// materialized. Retained state is bounded by distinct account PAIRS and
// the open CSV writer — account-pair scale, not transaction scale.
//
// Entity-scale tables come from exportEntities() after the fold; pairing
// the two reproduces exportAll() completely (test_windowed_e2e).
//
// CSV retirement arc: has_paid and account_flow_agg go through
// common::Table, so when Config::pgMirror is armed the same bytes
// stream into PostgreSQL directly. An EMPTY Config::outDir disables
// the file leg entirely (5b). The ledger CSV stays FILE-ONLY — its
// stem is "transactions", the streamed corpus table's name, and the
// canonical stream must never be overwritten by its dump.
//

#include "phantomledger/entities/accounts.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/ledger.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/schema.hpp"
#include "phantomledger/exporter/standard/aggregates.hpp"
#include "phantomledger/exporter/standard/transfers.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/synth/pii/membership_filter.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>

namespace PhantomLedger::exporter::standard {

class StreamingTransfersExport {
public:
  struct Config {
    const ::PhantomLedger::entity::account::Registry *registry = nullptr;
    const ::PhantomLedger::entity::account::Lookup *lookup = nullptr;

    // Must be constructed exactly as exportAll does — (population,
    // window, growth) — or the visible corpus diverges.
    ::PhantomLedger::synth::pii::Membership membership;

    ::PhantomLedger::time::Window window{};
    std::filesystem::path outDir; // empty => no files (PG-only run)
    bool showTransactions = false;

    // When set, has_paid / account_flow_agg are ALSO written directly
    // into PostgreSQL as the same bytes the CSV files receive (CSV
    // retirement arc). The ledger CSV is deliberately excluded (see
    // file comment).
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;
  };

  explicit StreamingTransfersExport(Config config)
      : config_(std::move(config)),
        binSpec_(flow_agg::detail::makeBinSpec(config_.window,
                                               flow_agg::detail::kDefaultBinDays)) {
    const bool files = !config_.outDir.empty();
    if (files) {
      std::filesystem::create_directories(config_.outDir);
    }
    target_ = common::TableTarget{.dir = config_.outDir,
                                  .pg = config_.pgMirror};
    if (config_.showTransactions && files) {
      const common::TableTarget fileOnly{.dir = config_.outDir, .pg = nullptr};
      ledger_.emplace(common::openTable(fileOnly, schema::kLedger));
    }
  }

  void beginSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txns) {
    for (const auto &tx : txns) {
      ++rows_;

      const auto srcOwner = ::PhantomLedger::synth::pii::ownerOf(
          *config_.registry, *config_.lookup, tx.source);
      const auto dstOwner = ::PhantomLedger::synth::pii::ownerOf(
          *config_.registry, *config_.lookup, tx.target);
      if (!config_.membership.activeAt(srcOwner, tx.timestamp) ||
          !config_.membership.activeAt(dstOwner, tx.timestamp)) {
        continue; // before a joiner endpoint existed
      }
      ++visibleRows_;

      detail::accumulateHasPaid(hasPaid_, tx);
      flow_agg::detail::accumulate(flowAgg_, tx, binSpec_);

      if (ledger_.has_value()) {
        common::detail::writeLedgerRow(*ledger_, tx);
      }
    }
  }

  void endSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void finish() {
    {
      auto w = common::openTable(target_, schema::kHasPaid);
      writeHasPaidAggregates(w, hasPaid_);
    }
    {
      auto w = common::openTable(target_, schema::kAccountFlowAggBin);
      flow_agg::writeAccountFlowAggAggregates(w, flowAgg_, binSpec_);
    }
    ledger_.reset(); // closes transactions.csv
  }

  [[nodiscard]] std::uint64_t rowsWritten() const noexcept { return rows_; }

  [[nodiscard]] std::uint64_t visibleRows() const noexcept {
    return visibleRows_;
  }

private:
  Config config_;
  flow_agg::detail::BinSpec binSpec_;
  common::TableTarget target_;

  detail::AggregateMap hasPaid_;
  flow_agg::detail::AggregateMap flowAgg_;

  std::optional<common::Table> ledger_;

  std::uint64_t rows_ = 0;
  std::uint64_t visibleRows_ = 0;
};

} // namespace PhantomLedger::exporter::standard
