#pragma once
//
// phantomledger/exporter/sinks/txn_readback.hpp
//
// Streaming, ordered read-back of the streamed `transactions` table —
// the consuming half of the PostgreSQL stream-identity contract
// (row_seq ordinal, sinks::Postgres, test_postgres). This is the
// corpus-store access path for derived analytics: PostgreSQL holds the
// full posted stream that bounded-memory runs never retain, and a scan
// in `ORDER BY row_seq` replays it in exact corpus order.
//
// Lives with the sinks (untangling round, 2026-07-19): every consumer
// is export-side (run_ledger's lockstep resume verification, the
// aml-txn-edges read-back bundle), and the decode leans on
// encoding::parseKey — an edge the primitives layer must not carry.
// The namespace stays PhantomLedger::postgres: this extends the same
// postgres access surface as primitives' Connection, from the layer
// that consumes it.
//
// DECODE FIDELITY (pinned by test_pg_readback):
//   keys     src_acct/dst_acct parse back to the exact entity::Key via
//            encoding::parseKey (same layout table as the writer)
//   amount   bit-exact double round-trip: the sink writes shortest
//            round-trip text (csv::Writer via std::to_chars) into
//            float8, and the scan reads under extra_float_digits = 3,
//            so re-accumulated float sums match the corpus fold
//            bit-for-bit when applied in row_seq order
//   ts       "YYYY-MM-DD HH:MM:SS" under DateStyle 'ISO, YMD', the
//            exact inverse of the ledger's formatTimestamp
//   fraud    is_fraud round-trips the flag byte
//   channel  the stored name (channels::name, validated unique) parses
//            back to the exact channels::Tag via channels::parse — the
//            currency-scoped CTR rule (31 CFR 1010.311) reads it
//
// The rendered id text is surfaced alongside the parsed key because
// derived IDs (alerts, CTRs) hash the RENDERED account string — using
// the stored text verbatim removes any re-render dependency.
//
// One scan at a time per connection: the scan owns a read-only
// transaction with a server-side cursor, so memory stays bounded at one
// fetch batch regardless of corpus size. Abandoning a scan mid-stream
// rolls the transaction back; the connection stays usable.
//

#include "phantomledger/entities/identifiers.hpp"
#include "phantomledger/primitives/postgres/connection.hpp"
#include "phantomledger/taxonomies/channels/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

struct pg_result;

namespace PhantomLedger::postgres {

// One decoded row of the streamed table: exactly the fields the ledger
// row reconstructs losslessly (ring_id/fraud_type/device/ip are
// rendered forms; consumers needing them use the world, not the table).
struct StreamTxnRow {
  std::uint64_t rowSeq = 0;
  ::PhantomLedger::entity::Key source{};
  ::PhantomLedger::entity::Key target{};
  double amount = 0.0;
  std::int64_t timestamp = 0;
  std::uint8_t fraudFlag = 0;
  ::PhantomLedger::channels::Tag channel{};

  // src_acct / dst_acct exactly as stored (hash inputs for derived IDs).
  std::string sourceRendered;
  std::string targetRendered;
};

struct StreamTxnBounds {
  std::uint64_t rows = 0;
  std::int64_t minTs = 0; // meaningful only when rows > 0
  std::int64_t maxTs = 0; // meaningful only when rows > 0
};

// "YYYY-MM-DD HH:MM:SS" (the ledger's formatTimestamp; PostgreSQL ISO
// timestamp output) -> epoch seconds. Throws Error on malformed input.
[[nodiscard]] std::int64_t parseLedgerTimestamp(std::string_view text);

// Row count and timestamp extent — the cheap first pass that gives
// derived analytics its sim window (30/90-day cuts) before the sweep.
[[nodiscard]] StreamTxnBounds
queryStreamBounds(Connection &conn, const std::string &table = "transactions");

class TransactionScan {
public:
  explicit TransactionScan(Connection &conn,
                           const std::string &table = "transactions");

  TransactionScan(const TransactionScan &) = delete;
  TransactionScan &operator=(const TransactionScan &) = delete;
  TransactionScan(TransactionScan &&) = delete;
  TransactionScan &operator=(TransactionScan &&) = delete;

  ~TransactionScan();

  // Decodes the next row in row_seq order. False once the stream ends
  // (the scan then commits its read-only transaction and detaches).
  [[nodiscard]] bool next(StreamTxnRow &out);

  [[nodiscard]] std::uint64_t rowsRead() const noexcept { return rows_; }

private:
  [[nodiscard]] bool fetchBatch();

  Connection *conn_;
  bool active_ = false;

  struct pg_result *batch_ = nullptr;
  int rowsInBatch_ = 0;
  int rowInBatch_ = 0;

  std::uint64_t rows_ = 0;
};

} // namespace PhantomLedger::postgres
