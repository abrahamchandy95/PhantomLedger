#pragma once
//
// phantomledger/exporter/aml_txn_edges/readback.hpp
//
// The PostgreSQL read-back twin of derived::buildBundle: builds the
// SAME Bundle from the streamed `transactions` table instead of a
// retained corpus, for bounded-memory runs that never materialize the
// posted stream in process.
//
// One code path, two corpus stores: queryStreamBounds supplies the sim
// window (the corpus path's min/max scan), then one TransactionScan in
// row_seq order feeds THE SAME derived::TxnSweep::observe, and THE SAME
// derived::finishBundle produces the bundle from the world + sweep.
// Byte parity with the corpus builder is therefore structural, resting
// on the pinned decode contract (test_pg_readback): row_seq equals the
// 1-based corpus index the alert/CTR IDs hash, amounts decode bit-exact
// so the flow/link float sums (including the 30/90-day sim-end windows)
// match bit-for-bit, and parsed keys re-render to the stored id text.
// Enforced end to end by test_derived_readback.
//
// Never SQL float aggregation (server-side SUM order is unspecified);
// never span_index (engine-specific bookkeeping).
//

#include "phantomledger/exporter/aml_txn_edges/derived.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"

#include <span>
#include <string>

namespace PhantomLedger::exporter::aml_txn_edges::readback {

[[nodiscard]] derived::Bundle
buildBundle(const pipeline::People &people, const pipeline::Holdings &holdings,
            std::span<const aml::sar::SarRecord> sars,
            ::PhantomLedger::postgres::Connection &conn,
            const std::string &table = "transactions");

} // namespace PhantomLedger::exporter::aml_txn_edges::readback
