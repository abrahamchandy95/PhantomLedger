#pragma once
//
// phantomledger/exporter/standard/streaming.hpp
//
// Streaming twin of the standard exporter's transaction-scale artifacts,
// for the windowed engine: a chunk sink that consumes settled spans as
// Phase B folds them and produces byte-identical tables to exportAll()'s
// corpus-based path:
//
//   has_paid            per-(source,target) aggregate — accumulated row
//                       by row in corpus order (so the floating-point
//                       sums are bit-identical), written SORTED at
//                       finish(), exactly like writeHasPaidRows
//   account_flow_agg    same, with fixed-width temporal bins
//
// The membership filter is applied per row (the same activeAt predicate
// filterByMembership uses — the [joinTs, closeTs) interval since H3
// part 3c-ii), so no visible-corpus copy is ever materialized. Retained
// state is bounded by distinct account PAIRS — account-pair scale, not
// transaction scale.
//
// Entity-scale tables come from exportEntities() after the fold; pairing
// the two reproduces exportAll() completely (test_windowed_e2e's
// successor gates: arch equivalence + the table golden).
//
// Both tables go through common::Table: when Config::pgMirror is armed
// the rendered bytes stream into PostgreSQL directly — the only
// production destination (no files). The raw ledger is the streamed
// 'transactions' corpus table itself, never a table here.
//

#include "phantomledger/entities/holdings/accounts.hpp"
#include "phantomledger/exporter/common/framework.hpp"
#include "phantomledger/exporter/common/table.hpp"
#include "phantomledger/exporter/csv.hpp"
#include "phantomledger/exporter/standard/aggregates.hpp"
#include "phantomledger/exporter/standard/membership_filter.hpp"
#include "phantomledger/exporter/standard/schema.hpp"
#include "phantomledger/exporter/standard/transfers.hpp"
#include "phantomledger/pipeline/chunk/schedule.hpp"
#include "phantomledger/primitives/time/window.hpp"
#include "phantomledger/synth/pii/membership.hpp"
#include "phantomledger/transactions/record.hpp"

#include <cstdint>
#include <span>
#include <utility>

namespace PhantomLedger::exporter::standard {

class StreamingTransfersExport {
public:
  struct Config {
    const ::PhantomLedger::entity::account::Registry *registry = nullptr;
    const ::PhantomLedger::entity::account::Lookup *lookup = nullptr;

    // Must be constructed exactly as exportAll does — through
    // synth::personas::join_cohort::membershipOf(pack, window), the
    // ONE construction path — or the visible corpus diverges.
    ::PhantomLedger::synth::pii::Membership membership;

    ::PhantomLedger::time::Window window{};

    // When set, has_paid / account_flow_agg are written directly into
    // PostgreSQL as the bytes the csv::Writer renders — the only
    // production destination.
    const ::PhantomLedger::exporter::sinks::PgMirror *pgMirror = nullptr;

    // Test infrastructure: rendered bytes per table stem.
    common::TableCapture *capture = nullptr;
  };

  explicit StreamingTransfersExport(Config config)
      : config_(std::move(config)),
        binSpec_(flow_agg::detail::makeBinSpec(config_.window,
                                               flow_agg::detail::kDefaultBinDays)) {
    target_ = common::TableTarget{.pg = config_.pgMirror,
                                  .capture = config_.capture};
  }

  void beginSpan(const ::PhantomLedger::pipeline::chunk::Span &) noexcept {}

  void append(std::span<const transactions::Transaction> txns) {
    for (const auto &tx : txns) {
      ++rows_;

      const auto srcOwner =
          ownerOf(*config_.registry, *config_.lookup, tx.source);
      const auto dstOwner =
          ownerOf(*config_.registry, *config_.lookup, tx.target);
      if (!config_.membership.activeAt(srcOwner, tx.timestamp) ||
          !config_.membership.activeAt(dstOwner, tx.timestamp)) {
        continue; // outside an endpoint owner's [joinTs, closeTs)
      }
      ++visibleRows_;

      detail::accumulateHasPaid(hasPaid_, tx);
      flow_agg::detail::accumulate(flowAgg_, tx, binSpec_);
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

  std::uint64_t rows_ = 0;
  std::uint64_t visibleRows_ = 0;
};

} // namespace PhantomLedger::exporter::standard
